
#ifndef GRAPE_WORKER_INGRESS_SYNC_TRAVERSAL_WORKER_H_
#define GRAPE_WORKER_INGRESS_SYNC_TRAVERSAL_WORKER_H_

#include <grape/fragment/loader.h>

#include <boost/mpi.hpp>
#include <boost/serialization/vector.hpp>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "flags.h"
#include "grape/app/traversal_app_base.h"
#include "grape/communication/communicator.h"
#include "grape/communication/sync_comm.h"
#include "grape/graph/adj_list.h"
#include "grape/parallel/parallel_engine.h"
#include "grape/parallel/parallel_message_manager.h"
#include "timer.h"

namespace grape {

template <typename FRAG_T, typename VALUE_T>
class IterateKernel;

/**
 * @brief A Worker manages the computation cycle. DefaultWorker is a kind of
 * worker for apps derived from AppBase.
 *
 * @tparam APP_T
 */

template <typename APP_T>
class IngressSyncTraversalWorker : public ParallelEngine {
  static_assert(std::is_base_of<TraversalAppBase<typename APP_T::fragment_t,
                                                 typename APP_T::value_t>,
                                APP_T>::value,
                "IngressSyncTraversalWorker should work with App");

 public:
  using fragment_t = typename APP_T::fragment_t;
  using value_t = typename APP_T::value_t;
  using delta_t = typename APP_T::delta_t;
  using vertex_t = typename APP_T::vertex_t;
  using message_manager_t = ParallelMessageManager;
  using oid_t = typename fragment_t::oid_t;
  using vid_t = typename APP_T::vid_t;

  IngressSyncTraversalWorker(std::shared_ptr<APP_T> app,
                             std::shared_ptr<fragment_t>& graph)
      : app_(app), fragment_(graph) {}

  void Init(const CommSpec& comm_spec,
            const ParallelEngineSpec& pe_spec = DefaultParallelEngineSpec()) {
    fragment_->PrepareToRunApp(APP_T::message_strategy,
                               APP_T::need_split_edges);

    comm_spec_ = comm_spec;

    // 等待所有worker执行完毕
    MPI_Barrier(comm_spec_.comm());

    // 初始化发消息相关的buffer
    messages_.Init(comm_spec_.comm());
    messages_.InitChannels(thread_num());
    communicator_.InitCommunicator(comm_spec.comm());

    InitParallelEngine(pe_spec);
    if (FLAGS_cilk) {
      LOG(INFO) << "Thread num: " << getWorkers();
    }
  }

  void Query() {
    MPI_Barrier(comm_spec_.comm());

    // allocate dependency arrays
    app_->Init(comm_spec_, fragment_);
   
    double size = 0;
    int sendcount = (fragment_->GetTotalVerticesNum()/comm_spec_.fnum()) * 0.3;
    bool threadIsend_ = true;    
    bool isTerminate = false;
    double global_value_sum = 0;
    bool last_global_valuesum = true;
    std::thread checkthread_;
    auto inner_local = fragment_->InnerLocalVertices();
    auto master_vertices = fragment_->MasterVertices(); 
    auto inner_vertices = fragment_->InnerVertices();
    auto outer_vertices = fragment_->OuterVertices();
    auto bound_vertices = fragment_->BoundVertices();


    auto& values = app_->values_;  //inner vertices
    auto& deltas = app_->deltas_;  //tvnum

    auto cmp = []( std::pair<vid_t,value_t>& a,const std::pair<vid_t,value_t>& b){return a.second > b.second;};
    std::priority_queue<std::pair<vid_t,value_t>, std::vector<std::pair<vid_t,value_t>>, decltype(cmp)>prio_queue_(cmp);

    int step = 0;
    bool batch_stage = true;
    double exec_time = 0;

    messages_.Start();

    messages_.InitChannels(thread_num());


    messages_.StartARound();
    while (true) {
      exec_time -= GetCurrentTime();
      
      
      app_->next_modified_.ParallelClear(thread_num());

      {
        messages_.ParallelProcess<fragment_t, DependencyData<vid_t, value_t>>(
            thread_num(), *fragment_,
            [this](int tid, vertex_t v,
                   const DependencyData<vid_t, value_t>& msg) {
              if (app_->AccumulateTo(v, msg)) {
                app_->curr_modified_.Insert(v);
                app_->send_modified_.Insert(v);
              }
            });
        
      }
      // double sumvalues = 0;
      // for (auto v : inner_vertices) {
      //     if (values[v] != app_->GetIdentityElement())
      //       sumvalues += values[v];
      // }
      // if (comm_spec_.fid() == 0) {
      //   LOG(INFO)<<"!!!!"<<std::to_string(sumvalues)<<" setp: "<<step<<" modified count: "<<app_->curr_modified_.Count();
      // }
      // Traverse outgoing neighbors
      
      ForEachCilk(
          app_->curr_modified_, inner_local, [this](int tid, vertex_t u) {
            auto& value = app_->values_[u];
            auto last_value = value;
            auto& delta = app_->deltas_[u];

            if (app_->CombineValueDelta(value, delta)) {
              app_->Compute(u, last_value, delta, app_->next_modified_, app_->send_modified_);
            }
      });
      ForEachCilk(
          app_->curr_modified_, master_vertices, [this](int tid, vertex_t u) {
            auto& value = app_->values_[u];
            auto last_value = value;
            auto& delta = app_->deltas_[u];
            if (app_->CombineValueDelta(value, delta)) {
              app_->Compute(u, last_value, delta, app_->next_modified_, app_->send_modified_);
            }
      });
      
      if (step < FLAGS_step && last_global_valuesum) {
        auto& channels = messages_.Channels();

        // send local delta to remote
        ForEach(app_->next_modified_, outer_vertices,
                [&channels, this](int tid, vertex_t v) {
                  auto& delta_to_send = app_->deltas_[v];

                  if (delta_to_send.value != app_->GetIdentityElement()) {
                    channels[tid].SyncStateOnOuterVertex(*fragment_, v,
                                                        delta_to_send);
                  }
                });

        ForEach(app_->next_modified_, master_vertices,
            [&channels, this](int tid, vertex_t v) {
              auto& delta_to_send = app_->deltas_[v];

              if (delta_to_send.value != app_->GetIdentityElement()) {
                channels[tid].SendMsgThroughFid(*fragment_, v,
                                                      delta_to_send);
              }
        });

        int sendsize = app_->next_modified_.Count();
        // if (app_->next_modified_.Count() > 0) {
        //   messages_.ForceContinue();
        // }

        VLOG(1) << "[Worker " << comm_spec_.worker_id()
                << "]: Finished IterateKernel - " << step;
        messages_.FinishARound();

        exec_time += GetCurrentTime();
        double msgSize = messages_.GetMsgSize();
        size += msgSize;
        if (threadIsend_) {
          threadIsend_ = false; 
          checkthread_ = std::thread(
          [this,&isTerminate, &values,&msgSize, &sendsize, &size,&threadIsend_,&global_value_sum]() {
            isTerminate = termCheck(values, msgSize, sendsize, size,global_value_sum);
            threadIsend_ = true;
          });
          checkthread_.detach();
        }
        
        if (isTerminate) {
          if (batch_stage) {
            batch_stage = false;

            if (comm_spec_.worker_id() == grape::kCoordinatorRank) {
              LOG(INFO) << "Batch time: " << exec_time << " sec";
              LOG(INFO) << "Total step:" << step;
            }
            break;
          } 
        }
        ++step;
        messages_.StartARound();
      }else {
        last_global_valuesum = false;
        if (app_->next_modified_.Count() == 0) {
          auto& channels = messages_.Channels();
          if (app_->send_modified_.Count() > sendcount) {
            //Statistical abs(lase_values-value) to priority queue
            for(auto u : bound_vertices) {
                  auto& delta = app_->deltas_[u].value;
                  if (delta != app_->GetIdentityElement()) {
                    if (app_->send_modified_.Exist(u)){
                      prio_queue_.push(std::pair<vid_t,value_t>(u.GetValue(),delta));
                    }
                  }
            }
  
            int size_que =prio_queue_.size();
          
            app_->send_modified_.ParallelClear(thread_num());
            int index = size_que * FLAGS_termcheck_threshold;
            //LOG(INFO)<<">>>>>>>>>>>"<<size_que<<">"<<index;
            for (int i = 0; i < size_que;i++) {
              if (index >= 0 && !prio_queue_.empty()) {
                std::pair<vid_t,value_t> p = prio_queue_.top();
                vertex_t v(p.first);
                // if (comm_spec_.fid() == 0)
                //   LOG(INFO)<<"........"<<p.first<<"====="<<p.second;
                app_->send_modified_.Insert(v);
                prio_queue_.pop();
                index--;
              }else {break;}
            }
          }

          // int countsent = app_->send_modified_.Count();
          // for (auto i : inner_local) {
          //   if (app_->send_modified_.Exist(i)){
          //     countsent--;
          //   }
          // }
          //LOG(INFO)<<" fid:"<<comm_spec_.fid() <<" total send size: "<<countsent;
          //LOG(INFO)<<" fid:"<<comm_spec_.fid() <<" total send size: "<<app_->send_modified_.Count();
          // send local delta to remote
          ForEach(app_->send_modified_,outer_vertices,
                  [&channels, this](int tid, vertex_t v) {
                    auto& delta_to_send = app_->deltas_[v];
                  
                    if (delta_to_send.value != app_->GetIdentityElement()) {
                      channels[tid].SyncStateOnOuterVertex(*fragment_, v,
                                                          delta_to_send);
                    }
                  });

          ForEach(app_->send_modified_,master_vertices,
              [&channels, this](int tid, vertex_t v) {
                auto& delta_to_send = app_->deltas_[v];

                if (delta_to_send.value != app_->GetIdentityElement()) {
                  channels[tid].SendMsgThroughFid(*fragment_, v,
                                                        delta_to_send);
                }
          });

          int sendsize = app_->send_modified_.Count();
            
         // LOG(INFO)<<"sendsize "<<sendsize;
          app_->send_modified_.ParallelClear(thread_num());
        

          VLOG(1) << "[Worker " << comm_spec_.worker_id()
                  << "]: Finished IterateKernel - " << step;
          messages_.FinishARound();

          exec_time += GetCurrentTime();
          double msgSize = messages_.GetMsgSize();
          size += msgSize;
          if (threadIsend_) {
            threadIsend_ = false; 
            checkthread_ = std::thread(
            [this,&isTerminate, &values,&msgSize, &sendsize, &size,&threadIsend_,&global_value_sum]() {
              isTerminate = termCheck(values, msgSize, sendsize, size,global_value_sum);
              threadIsend_ = true;
            });
            checkthread_.detach();
          }
          
          if (isTerminate) {
            if (batch_stage) {
              batch_stage = false;

              if (comm_spec_.worker_id() == grape::kCoordinatorRank) {
                LOG(INFO) << "Batch time: " << exec_time << " sec";
                LOG(INFO) << "Total step:" << step;
              }
              break;
            } 
          }
          ++step;
          messages_.StartARound();
        }
      }
      app_->next_modified_.Swap(app_->curr_modified_);
    }
    MPI_Barrier(comm_spec_.comm());
  }

  void Output(std::ostream& os) {
    auto inner_vertices = fragment_->InnerVertices();
    auto& values = app_->values_;

    for (auto v : inner_vertices) {
      os << fragment_->GetId(v) << " " << values[v] << std::endl;
    }
  }

  void Finalize() { messages_.Finalize(); }

 private:
   bool termCheck(VertexArray<value_t, vid_t>& values,
                  double& msgSize,
                  int& sendsize,
                  double& size,
                  double& global_value_sum) {
    auto vertices = fragment_->InnerVertices();
    global_value_sum = 0;
    double global_msgsize = 0;
    double global_sent = 0;
    double diff_value = 0;
    int global_modified = std::numeric_limits<int>::max();

    for (auto u : vertices) {
      if (values[u] != app_->GetIdentityElement())
        diff_value += values[u];
    }
    communicator_.template Sum(diff_value, global_value_sum);
    communicator_.template Sum(size, global_sent);
    communicator_.template Sum(sendsize, global_modified);
    communicator_.template Sum(msgSize, global_msgsize);

    if (comm_spec_.worker_id() == grape::kCoordinatorRank) {
      LOG(INFO) << "Values:"<< std::to_string(global_value_sum);
      LOG(INFO) << "SendSize:"<<(global_sent/std::pow(1024,3));
      LOG(INFO) << "Modified:"<<global_modified;
    }
   
    if (FLAGS_termcheck_value_threshold != 0)
      return global_value_sum > FLAGS_termcheck_value_threshold;
    else 
      return (global_modified == 0 && global_msgsize == 0);
  }


  std::shared_ptr<APP_T> app_;
  std::shared_ptr<fragment_t> fragment_;
  message_manager_t messages_;
  Communicator communicator_;
  CommSpec comm_spec_;
};

}  // namespace grape

#endif
