#ifndef GRAPE_WORKER_INGRESS_SYNC_ITER_WORKER_H_
#define GRAPE_WORKER_INGRESS_SYNC_ITER_WORKER_H_

#include <grape/fragment/loader.h>

#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include "flags.h"
#include <helib/helib.h>
#include "grape/app/ingress_app_base.h"
#include "grape/communication/communicator.h"
#include "grape/communication/sync_comm.h"
#include "grape/fragment/inc_fragment_builder.h"
#include "grape/graph/adj_list.h"
#include "grape/parallel/default_message_manager.h"
#include "grape/parallel/parallel.h"
#include "grape/parallel/parallel_engine.h"
#include "timer.h"

using namespace helib;
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
class IngressSyncIterWorker : public ParallelEngine {
  static_assert(std::is_base_of<IterateKernel<typename APP_T::fragment_t,
                                              typename APP_T::value_t>,
                                APP_T>::value,
                "IngressSyncIterWorker should work with App");

 public:
  using fragment_t = typename APP_T::fragment_t;
  using value_t = typename APP_T::value_t;
  using vertex_t = typename APP_T::vertex_t;
  using message_manager_t = ParallelMessageManager;
  using vid_t = typename APP_T::vid_t;

  IngressSyncIterWorker(std::shared_ptr<APP_T> app,
                        std::shared_ptr<fragment_t>& graph)
      : app_(app), graph_(graph) {}

  void Init(const CommSpec& comm_spec,
            const ParallelEngineSpec& pe_spec = DefaultParallelEngineSpec()) {
    graph_->PrepareToRunApp(APP_T::message_strategy, APP_T::need_split_edges);

    comm_spec_ = comm_spec;

     //kCoordinatorRank generate public key and secret key. then broadcast public key to others
    {
      helib::Context context = helib::ContextBuilder<helib::CKKS>()
                               .m(16)
                               .precision(20)
                               .bits(119)
                               .c(2)
                               .build();

    std::string pkey;
    std::string skey;
      if (comm_spec_.worker_id() == grape::kCoordinatorRank) {
        LOG(INFO)<<"securityLevel=" << context.securityLevel();
        
        SecKey secretKey(context);
        //生成密钥
        secretKey.GenSecKey();
        //生成公钥
        const PubKey& publicKey = secretKey;
        //公钥生成序列化
        std::stringstream buffer;
        publicKey.writeToJSON(buffer);
        buffer >> pkey;
        // //广播公钥
        BcastSend<std::string>(pkey, comm_spec_.comm());

        buffer.clear();
        secretKey.writeToJSON(buffer);
        buffer >> skey;

        BcastSend<std::string>(skey, comm_spec_.comm());
      } else {
        BcastRecv<std::string>(pkey, comm_spec_.comm(),grape::kCoordinatorRank);
        BcastRecv<std::string>(skey, comm_spec_.comm(),grape::kCoordinatorRank);
      }
  
      std::stringstream buffer_pkey;
      buffer_pkey << pkey;
      PubKey publicKey = helib::PubKey::readFromJSON(buffer_pkey, context);
    
      std::stringstream buffer_skey;
      buffer_skey << skey;
      SecKey secretKey = helib::SecKey::readFromJSON(buffer_skey,context);
      std::vector<double> v0(1);
      v0[0] = 4.1;
      PtxtArray p0(context, v0);

      Ctxt c0(publicKey);
      p0.encrypt(c0);
      double i = comm_spec_.worker_id() + 1;
      c0 += i;
      p0.decrypt(c0,secretKey);
      std::vector<double> v1;
      p0.store(v1);
      LOG(INFO)<<"<<<<<<<<<<<v0"<<v1[0];
    }

    // 等待所有worker执行完毕
    MPI_Barrier(comm_spec_.comm());

    // 初始化发消息相关的buffer
    messages_.Init(comm_spec_.comm());
    messages_.InitChannels(thread_num()); //each channel is a thread local message buffer.
    communicator_.InitCommunicator(comm_spec.comm());
    terminate_checking_time_ = 0;

    InitParallelEngine(pe_spec);
    LOG(INFO) << "Thread num: " << thread_num();
  }

  void Query() {
    MPI_Barrier(comm_spec_.comm());
    app_->Init(comm_spec_, *graph_, false);

    if (FLAGS_debug) {
      volatile int i = 0;
      char hostname[256];
      gethostname(hostname, sizeof(hostname));
      printf("PID %d on %s ready for attach\n", getpid(), hostname);
      fflush(stdout);
      while (0 == i) {
        sleep(1);
      }
    }
    auto inner_local = graph_->InnerLocalVertices();  //range of inner local lid
    auto master_vertices = graph_->MasterVertices();   //range of master lid
    auto outer_vertices = graph_->OuterVertices();   //range of outer lid
    auto inner_vertices = graph_->InnerVertices(); //range of inner + master
    auto& values = app_->values_;  //inner vertices
    auto& deltas = app_->deltas_;  //tvnum
    auto& master_deltas = app_->master_deltas_;  //master vertices
    auto& send_master_deltas = app_->send_master_deltas_;
    //    auto& prioritys = app_->priority_;
    VertexArray<value_t, vid_t> last_values;

    int step = 1;
    bool batch_stage = true;

    auto init_time = GetCurrentTime();

    last_values.Init(inner_vertices);

    LOG(INFO) << "Init Arraytime: " << GetCurrentTime() - init_time;

    for (auto v : inner_vertices) {
      value_t val;
      app_->init_v(v, val);
      last_values[v] = val;
    }

    double exec_time = 0;
    // start receive thread,At runtime, the receiving thread is all running
    messages_.Start();
    // Run an empty round, otherwise ParallelProcess will stuck
    //messages_.StartARound();
    messages_.InitChannels(thread_num());
    //messages_.FinishARound();

    app_->iterate_begin(*graph_); //do nothing 
    // for (auto v : inner_vertices) {
    //     std::string s =" fid:"+std::to_string(comm_spec_.fid())+" oid: "+std::to_string(graph_->GetId(v)) +" oes: ";
    //     auto oes = graph_->GetOutgoingAdjList(v);
    //     for (auto it : oes) {
    //       vid_t oid = graph_->GetId(it.neighbor);
    //       s+= "  "  + std::to_string(oid);
    //    }
    //    LOG(INFO)<<" <<"<<s;
    // }

    while (true) {
      exec_time -= GetCurrentTime();
      messages_.StartARound(); //等待上轮发送线程结束，清空本地接收线程的消息，向其他worker发送消息
    
      auto& channels = messages_.Channels();
    
      {
        auto begin = GetCurrentTime();  
            

        messages_.ParallelProcess<fragment_t, value_t>(  //receive delta
            thread_num(), *graph_,
            [this](int tid, vertex_t v, value_t received_delta) {
              if (graph_->IsMasterVertex(v)){
                app_->accumulate(app_->master_deltas_[v], received_delta);
              }else {
                app_->accumulate(app_->deltas_[v], received_delta);
              }
            });
        VLOG(1) << "Process time: " << GetCurrentTime() - begin;
      }
   

     
      {
        auto begin = GetCurrentTime();
        if (FLAGS_cilk) {
#ifdef INTERNAL_PARALLEL
          LOG(FATAL) << "Ingress is not compiled with -DUSE_CILK";
#endif
          parallel_for(vid_t i = inner_local.begin().GetValue();
                       i < inner_local.end().GetValue(); i++) {
            vertex_t u(i);
            auto& value = values[u];
            auto delta = atomic_exch(deltas[u], app_->default_v());
            auto oes = graph_->GetOutgoingAdjList(u);
            app_->g_function(*graph_, u, value, delta, oes);
            app_->accumulate(value, delta);
          }
         } //else {
        //   ForEach(inner_vertices,
        //           [this, &values, &deltas](int tid, vertex_t u) {
        //             // app_->priority(prioritys[u], values[u],
        //             //    deltas[u]); if (abs(prioritys[u]) > pri) {
        //             auto& value = values[u];
        //             auto delta = atomic_exch(deltas[u], app_->default_v());
        //             auto oes = graph_->GetOutgoingAdjList(u);
                   
        //             app_->g_function(*graph_, u, value, delta, oes);
        //             app_->accumulate(value, delta);
        //             // }
        //           });
        // }
        {
             // send local delta to remote
          ForEach(outer_vertices, [this, &deltas, &channels](int tid,
                                                            vertex_t v) {
            auto& delta_to_send = deltas[v];
            if (delta_to_send != app_->default_v()) {
              channels[tid].template SyncStateOnOuterVertex<fragment_t, value_t>(
                  *graph_, v, delta_to_send);
              delta_to_send = app_->default_v();
            }
          });
        }
     
         if (FLAGS_cilk) {
#ifdef INTERNAL_PARALLEL
          LOG(FATAL) << "Ingress is not compiled with -DUSE_CILK";
#endif
          parallel_for(vid_t i = master_vertices.begin().GetValue();
                       i < master_vertices.end().GetValue(); i++) {
            vertex_t u(i);
            auto& value = values[u];
            auto delta = atomic_exch(deltas[u], app_->default_v());
            auto mdelta = atomic_exch(master_deltas[u],app_->default_v());
            atomic_add(send_master_deltas[u], delta);// send_master_deltas[u] += delta; //will send master delta to remote
            delta += mdelta;
            auto oes = graph_->GetOutgoingAdjList(u);
            if (!oes.Empty()){
               app_->g_function(*graph_, u, value, delta, oes);
            }
            app_->accumulate(value, delta);
          
          }
        }
  
        VLOG(1) << "Iter time: " << GetCurrentTime() - begin;
      }

      {
        auto begin = GetCurrentTime();

        //sync master delta to remote
        ForEach(master_vertices, [this, &send_master_deltas, &channels](int tid,
                                                           vertex_t v) {
          auto& delta_to_send = send_master_deltas[v];
          if (delta_to_send != app_->default_v()) {
            channels[tid].template SendMsgThroughFid<fragment_t, value_t>(
                *graph_, v, delta_to_send);
            delta_to_send = app_->default_v();
          }
        });

        VLOG(1) << "Send time: " << GetCurrentTime() - begin;
      }
      
      VLOG(1) << "[Worker " << comm_spec_.worker_id()
              << "]: Finished IterateKernel - " << step;

      // default_work,同步一轮
      messages_.FinishARound();  //local channel 将所有消息发送给全局的发送队列，send producer 清零，receive producer reset

      exec_time += GetCurrentTime();

      if (termCheck(last_values, values)) {
        if (batch_stage) {
          batch_stage = false;

          if (comm_spec_.worker_id() == grape::kCoordinatorRank) {
            LOG(INFO) << "Batch time: " << exec_time << " sec";
            LOG(INFO)<<"Total step:"<<step;
          }
          break;
        }
      }
      LOG(INFO)<<comm_spec_.fid()<<"<<<"<<step;
      ++step;
      
    }
    MPI_Barrier(comm_spec_.comm());
  }
  
  void Output(std::ostream& os) {
    auto inner_vertices = graph_->InnerVertices();
    auto& values = app_->values_;

    for (auto v : inner_vertices) {
      os << graph_->GetId(v) << " " << values[v] << std::endl;
    }
  }

  void Finalize() { messages_.Finalize(); }

 private:
  bool termCheck(VertexArray<value_t, vid_t>& last_values,
                 VertexArray<value_t, vid_t>& values) {
    terminate_checking_time_ -= GetCurrentTime();
    auto vertices = graph_->InnerVertices();
    double diff_sum = 0, global_diff_sum,diff_value = 0,global_value_sum;

    for (auto u : vertices) {
      diff_value += values[u];
      diff_sum += fabs(last_values[u] - values[u]);
      last_values[u] = values[u];
    }

    communicator_.template Sum(diff_sum, global_diff_sum);
    communicator_.template Sum(diff_value, global_value_sum);

    if (comm_spec_.worker_id() == grape::kCoordinatorRank) {
      LOG(INFO) << "Diff: " << global_diff_sum;
      LOG(INFO) << "Values:"<< global_value_sum;
    }

    terminate_checking_time_ += GetCurrentTime();

    return global_diff_sum < FLAGS_termcheck_threshold;
  }

  std::shared_ptr<APP_T> app_;
  std::shared_ptr<fragment_t>& graph_;
  message_manager_t messages_;
  Communicator communicator_;
  CommSpec comm_spec_;
  double terminate_checking_time_;

  class compare_priority {
   public:
    VertexArray<value_t, vid_t>& parent;

    explicit compare_priority(VertexArray<value_t, vid_t>& inparent)
        : parent(inparent) {}

    bool operator()(const vid_t a, const vid_t b) {
      return abs(parent[Vertex<unsigned int>(a)]) >
             abs(parent[Vertex<unsigned int>(b)]);
    }
  };
};

}  // namespace grape

#endif  // GRAPE_WORKER_ASYNC_WORKER_H_
