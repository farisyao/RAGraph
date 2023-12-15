/** Copyright 2020 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef GRAPE_PARALLEL_PARALLEL_MESSAGE_MANAGER_H_
#define GRAPE_PARALLEL_PARALLEL_MESSAGE_MANAGER_H_

#include <mpi.h>

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "grape/communication/sync_comm.h"
#include "grape/parallel/message_manager_base.h"
#include "grape/parallel/thread_local_message_buffer.h"
#include "grape/serialization/in_archive.h"
#include "grape/serialization/out_archive.h"
#include "grape/utils/concurrent_queue.h"
#include "grape/worker/comm_spec.h"
#include "flat_hash_map/flat_hash_map.hpp"


template <typename Key, typename Value>
using HashMap = ska::flat_hash_map<Key, Value>;

namespace grape {

/**
 * @brief A kind of parallel message manager.
 *
 * ParallelMessageManager support multi-threads to send messages concurrently
 * with channels. Each channel contains a thread local message buffer.
 *
 * For each thread local message buffer, when accumulated a given amount of
 * messages, the buffer will be sent through MPI.
 *
 * After a round of evaluation, there is a global barrier to determine whether
 * the fixed point is reached.
 *
 */

class ParallelMessageManager : public MessageManagerBase {
  static constexpr size_t default_msg_send_block_size = 2 * 1023 * 1024;
  static constexpr size_t default_msg_send_block_capacity = 2 * 1023 * 1024;

 public:
  ParallelMessageManager() : comm_(NULL_COMM) {}
  ~ParallelMessageManager() override {
    if (ValidComm(comm_)) {
      MPI_Comm_free(&comm_);
    }
  }

  /**
   * @brief Inherit
   */
  void Init(MPI_Comm comm) override {
    MPI_Comm_dup(comm, &comm_);
    comm_spec_.Init(comm_);
    fid_ = comm_spec_.fid();
    fnum_ = comm_spec_.fnum();

    recv_queues_.SetProducerNum(0);
    // recv_queues_[1].SetProducerNum(fnum_);

    round_ = 0;
    terminate_ = false;
    sent_size_ = 0;
  }
  int Rount() { return round_;}
  /**
   * @brief Inherit
   */
  void Start() override { startRecvThread(); }

  void TerminateCheck() { startTermcheck();  }
  /**
   * @diff_sum local values difference sum
   */
  void SendDiff(double& diff_sum) {
      int flag = 0;
      MPI_Request req;
      MPI_Isend(&diff_sum, 1, MPI_DOUBLE, 0, grape::TagTerminateCheck, comm_,&req);
      MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
    
  }

  /**
   * @brief Inherit
   */
   
  void StartARound() override {
    if (round_ != 0) {
      waitSend();
      auto& rq = recv_queues_;
      if (!to_self_.empty()) {  
        for (auto& iarc : to_self_) {
          OutArchive oarc(std::move(iarc));
          rq.Put(std::move(oarc));
        }
        to_self_.clear();
      }
     // rq.DecProducerNum();//减去本地的producer
    }
    sent_size_ = 0;
    startSendThread();
  }

  /**
   * @brief Inherit
   */
  void FinishARound() override {
    sent_size_ = finishMsgFilling();
    resetRecvQueue();
    round_++;
  }

  /**
   * @brief Inherit
   */
  bool ToTerminate() override {
    int flag = 1;
    if (sent_size_ == 0 && !force_continue_) {
      flag = 0;
    }
    int ret;
    MPI_Allreduce(&flag, &ret, 1, MPI_INT, MPI_SUM, comm_);
    return (ret == 0);
  }

  /**
   * @brief Inherit
   */
  void Finalize() override {
    waitSend();
    MPI_Barrier(comm_);

    stopRecvThread();
    MPI_Comm_free(&comm_);
    comm_ = NULL_COMM;
  }

  /**
   * @brief Inherit
   */
  void ForceContinue() override { force_continue_ = true; }

  bool IsTerminate() { return terminate_; }

  /**
   * @brief Inherit
   */
  size_t GetMsgSize() const override { return sent_size_; }

  /**
   * @brief Init a set of channels, each channel is a thread local message
   * buffer.
   *
   * @param channel_num Number of channels.
   * @param block_size Size of each channel.
   * @param block_cap Capacity of each channel.
   */
  void InitChannels(int channel_num = 1,
                    size_t block_size = default_msg_send_block_size,
                    size_t block_cap = default_msg_send_block_capacity) {
    channels_.resize(channel_num);
    for (auto& channel : channels_) {
      channel.Init(fnum_, this, block_size, block_cap);
    }
  }

  std::vector<ThreadLocalMessageBuffer<ParallelMessageManager>>& Channels() {
    return channels_;
  }

  /**
   * @brief Send a buffer to a fragment.
   *
   * @param fid Destination fragment id.
   * @param arc Message buffer.
   */
  inline void SendRawMsgByFid(fid_t fid, InArchive&& arc) {
    std::pair<fid_t, InArchive> item;
    item.first = fid;
    item.second = std::move(arc);
    sending_queue_.Put(std::move(item));
  }

  /**
   * @brief SyncStateOnOuterVertex on a channel.
   *
   * @tparam GRAPH_T Graph type.
   * @tparam MESSAGE_T Message type.
   * @param frag Source fragment.
   * @param v Source vertex.
   * @param msg
   * @param channel_id
   */
  template <typename GRAPH_T, typename MESSAGE_T>
  inline void SyncStateOnOuterVertex(const GRAPH_T& frag,
                                     const typename GRAPH_T::vertex_t& v,
                                     const MESSAGE_T& msg, int channel_id = 0) {
    channels_[channel_id].SyncStateOnOuterVertex<GRAPH_T, MESSAGE_T>(frag, v,
                                                                     msg);
  }

  template <typename GRAPH_T>
  inline void SyncStateOnOuterVertex(const GRAPH_T& frag,
                                     const typename GRAPH_T::vertex_t& v,
                                     int channel_id = 0) {
    channels_[channel_id].SyncStateOnOuterVertex<GRAPH_T>(frag, v);
  }

  /**
   * @brief SendMsgThroughIEdges on a channel.
   *
   * @tparam GRAPH_T Graph type.
   * @tparam MESSAGE_T Message type.
   * @param frag Source fragment.
   * @param v Source vertex.
   * @param msg
   * @param channel_id
   */
  template <typename GRAPH_T, typename MESSAGE_T>
  inline void SendMsgThroughIEdges(const GRAPH_T& frag,
                                   const typename GRAPH_T::vertex_t& v,
                                   const MESSAGE_T& msg, int channel_id = 0) {
    channels_[channel_id].SendMsgThroughIEdges<GRAPH_T, MESSAGE_T>(frag, v,
                                                                   msg);
  }

  /**
   * @brief SendMsgThroughOEdges on a channel.
   *
   * @tparam GRAPH_T Graph type.
   * @tparam MESSAGE_T Message type.
   * @param frag Source fragment.
   * @param v Source vertex.
   * @param msg
   * @param channel_id
   */
  template <typename GRAPH_T, typename MESSAGE_T>
  inline void SendMsgThroughOEdges(const GRAPH_T& frag,
                                   const typename GRAPH_T::vertex_t& v,
                                   const MESSAGE_T& msg, int channel_id = 0) {
    channels_[channel_id].SendMsgThroughOEdges<GRAPH_T, MESSAGE_T>(frag, v,
                                                                   msg);
  }

  /**
   * @brief SendMsgThroughEdges on a channel.
   *
   * @tparam GRAPH_T Graph type.
   * @tparam MESSAGE_T Message type.
   * @param frag Source fragment.
   * @param v Source vertex.
   * @param msg
   * @param channel_id
   */
  template <typename GRAPH_T, typename MESSAGE_T>
  inline void SendMsgThroughEdges(const GRAPH_T& frag,
                                  const typename GRAPH_T::vertex_t& v,
                                  const MESSAGE_T& msg, int channel_id = 0) {
    channels_[channel_id].SendMsgThroughEdges<GRAPH_T, MESSAGE_T>(frag, v, msg);
  }

  /**
   * @brief Parallel process all incoming messages with given function of last
   * round.
   *
   * @tparam GRAPH_T Graph type.
   * @tparam MESSAGE_T Message type.
   * @tparam FUNC_T Function type.
   * @param thread_num Number of threads.
   * @param frag
   * @param func
   */
  template <typename GRAPH_T, typename MESSAGE_T, typename FUNC_T>
  inline void ParallelProcess(int thread_num, const GRAPH_T& frag,
                              const FUNC_T& func) {
    std::vector<std::thread> threads(thread_num);

    for (int i = 0; i < thread_num; ++i) {
      threads[i] = std::thread(
          [&](int tid) {
            typename GRAPH_T::vid_t id;
            typename GRAPH_T::vertex_t vertex;
            MESSAGE_T msg;
            auto& que = recv_queues_;
            OutArchive arc;
            while (que.Get(arc)) {
              while (!arc.Empty()) {
                arc >> id >> msg;
                CHECK(frag.Gid2Vertex(id, vertex));
                func(tid, vertex, msg);
                //LOG(INFO)<<comm_spec_.fid()<<id;
              }
              
            }
          },
          i);
    }

    for (auto& thrd : threads) {
      thrd.join();
    }
  }

  /**
   * @brief Parallel process all incoming messages with given function of last
   * round.
   *
   * @tparam GRAPH_T Graph type.
   * @tparam MESSAGE_T Message type.
   * @tparam FUNC_T Function type.
   * @param thread_num Number of threads.
   * @param frag
   * @param func
   */
  template <typename MESSAGE_T, typename FUNC_T>
  inline void ParallelProcess(int thread_num, const FUNC_T& func) {
    std::vector<std::thread> threads(thread_num);

    for (int i = 0; i < thread_num; ++i) {
      threads[i] = std::thread(
          [&](int tid) {
            MESSAGE_T msg;
            auto& que = recv_queues_;
            OutArchive arc;
            while (que.Get(arc)) {
              while (!arc.Empty()) {
                arc >> msg;
                func(tid, msg);
              }
            }
          },
          i);
    }

    for (auto& thrd : threads) {
      thrd.join();
    }
  }

 private:
  void startSendThread() {
    force_continue_ = false;
    //int round = round_;

    CHECK_EQ(sending_queue_.Size(), 0);
    sending_queue_.SetProducerNum(1);
    send_thread_ = std::thread(
        [this]() {
          std::vector<MPI_Request> reqs;
          std::pair<fid_t, InArchive> item;
          int i = 0;
          while (sending_queue_.Get(item)) {
            if (item.second.GetSize() == 0) {
              continue;
            }
            if (item.first == fid_) {
              to_self_.emplace_back(std::move(item.second));
            } else {
              
              MPI_Request req;
              MPI_Isend(item.second.GetBuffer(), item.second.GetSize(),
                        MPI_CHAR, comm_spec_.FragToWorker(item.first),
                        grape::TagSendMessage, comm_, &req);
              // if (comm_spec_.worker_id() == 0)
              // LOG(INFO)<<round_<<"=============="<<++i;
              reqs.push_back(req);
              to_others_.emplace_back(std::move(item.second));

              MPI_Isend(NULL, 0, MPI_CHAR, comm_spec_.FragToWorker(item.first), grape::TagSendMessage,
                        comm_, &req);
              reqs.push_back(req);
            }
          }
          // for (fid_t i = 0; i < fnum_; ++i) {
          //   if (i == fid_) {
          //     continue;
          //   }
          //   MPI_Request req;
          //   MPI_Isend(NULL, 0, MPI_CHAR, comm_spec_.FragToWorker(i), grape::TagSendMessage,
          //             comm_, &req);
          //   reqs.push_back(req);
          // }
          MPI_Waitall(reqs.size(), &reqs[0], MPI_STATUSES_IGNORE);
          to_others_.clear();
        });
  }

  void probeAllIncomingMessages() {
    MPI_Status status;
    while (true) {
      MPI_Probe(MPI_ANY_SOURCE, grape::TagSendMessage, comm_, &status);
      if (status.MPI_SOURCE == comm_spec_.worker_id()) {
        MPI_Recv(NULL, 0, MPI_CHAR, status.MPI_SOURCE, grape::TagSendMessage, comm_,
                 MPI_STATUS_IGNORE);
        return;
      }
      int count;
      MPI_Get_count(&status, MPI_CHAR, &count);
      if (count == 0) {
        MPI_Recv(NULL, 0, MPI_CHAR, status.MPI_SOURCE, grape::TagSendMessage, comm_,
                 MPI_STATUS_IGNORE);
        recv_queues_.SetProducerNum(0);
      } else {
        OutArchive arc(count);
        MPI_Recv(arc.GetBuffer(), count, MPI_CHAR, status.MPI_SOURCE, grape::TagSendMessage,
                 comm_, MPI_STATUS_IGNORE);
        recv_queues_.Put(std::move(arc));
        // if (comm_spec_.worker_id() == 0)
        // LOG(INFO)<<"<<<<<<<<<<<"<<recv_queues_.Size();
      }
    }
  }

  int probeIncomingMessages() {
    int gotMessage = 0;
    int flag;
    MPI_Status status;
    while (true) {
      MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, comm_, &flag, &status);
      if (flag) {
        if (status.MPI_SOURCE == comm_spec_.worker_id()) {
          MPI_Recv(NULL, 0, MPI_CHAR, status.MPI_SOURCE, 0, comm_,
                   MPI_STATUS_IGNORE);
          return -1;
        }
        gotMessage = 1;
        int tag = status.MPI_TAG;
        int count;
        MPI_Get_count(&status, MPI_CHAR, &count);
        if (count == 0) {
          MPI_Recv(NULL, 0, MPI_CHAR, status.MPI_SOURCE, tag, comm_,
                   MPI_STATUS_IGNORE);
          //recv_queues_[tag % 2].DecProducerNum();
        } else {
          OutArchive arc(count);
          MPI_Recv(arc.GetBuffer(), count, MPI_CHAR, status.MPI_SOURCE, tag,
                   comm_, MPI_STATUS_IGNORE);
          recv_queues_.Put(std::move(arc));
        }
      } else {
        break;
      }
    }
    return gotMessage;
  }

  void CheckTerminateFromWorker() {
    
    if(comm_spec_.worker_id() == grape::kCoordinatorRank) {
      HashMap<int, double>fmap;
      while(true) {
        double global_dif = 0;
        MPI_Status status;
        while (true) {
          MPI_Probe(MPI_ANY_SOURCE, grape::TagTerminateCheck, comm_, &status);
          int src = status.MPI_SOURCE;
          double got_msg;
          MPI_Recv(&got_msg, 1, MPI_DOUBLE, src, grape::TagTerminateCheck, comm_,MPI_STATUS_IGNORE);
          fmap[src] = got_msg;
          if (fmap.size() == comm_spec_.fnum()) {
            for (auto i : fmap)
              global_dif += i.second;
              
            LOG(INFO)<<"Diff:"<<global_dif;
            fmap.clear();
            break;
          } 
        }
        if (global_dif < FLAGS_termcheck_threshold) {
          terminate_ = true;
          for (int dst_worker = 1; dst_worker < comm_spec_.worker_num(); ++dst_worker) {
            int arc = 1;
            MPI_Send(&arc, 1, MPI_INT, dst_worker, grape::TagTerminated, comm_);
          }
          return;
        }
      }
    } else {
        MPI_Status status;
        while(true) {
        MPI_Probe(MPI_ANY_SOURCE, grape::TagTerminated, comm_, &status);
        if (status.MPI_SOURCE == 0) {
          int orc = 0;
          MPI_Recv(&orc, 1, MPI_INT, 0, grape::TagTerminated, comm_, MPI_STATUS_IGNORE);
          if (orc == 1) {
            terminate_ = true;
            return;
          }
        }
      }
    }
  }
  void startTermcheck() {
     termcheck_thread_ = std::thread([this]() {
       CheckTerminateFromWorker();    
    });
  }
  void startRecvThread() {
    recv_thread_ = std::thread([this]() {
#if 0
      int idle_time = 0;
      while (true) {
        int gotMessage = probeIncomingMessages();
        if (gotMessage == -1) {
          break;
        }
        idle_time += static_cast<int>(gotMessage == 0);
        if (idle_time > 10) {
          poll(NULL, 0, 1);
          idle_time = 0;
        } else if (gotMessage == 0) {
#if __APPLE__
          sched_yield();
#else
          pthread_yield();
#endif
        }
      }
#else
      probeAllIncomingMessages();
#endif
    });
  }
  
  void stopRecvThread() {
    MPI_Send(NULL, 0, MPI_CHAR, comm_spec_.worker_id(), grape::TagSendMessage, comm_);
    recv_thread_.join();
    // termcheck_thread_.join();
  }

  inline size_t finishMsgFilling() {
    size_t ret = 0;
    for (auto& channel : channels_) {
      channel.FlushMessages();
      ret += channel.SentMsgSize();
      channel.Reset();
    }
    sending_queue_.DecProducerNum();
    return ret;
  }

  void resetRecvQueue() {
    auto& curr_recv_queue = recv_queues_;
    // if (round_) {
    //   OutArchive arc;
    //   while (curr_recv_queue.Get(arc)) {}
    // }
    if(FLAGS_application == "pagerank" || FLAGS_application == "php")
      curr_recv_queue.SetProducerNum(1);
    else
      curr_recv_queue.SetProducerNum(0);
  }

 

  void waitSend() { send_thread_.join(); }

  fid_t fid_;
  fid_t fnum_;
  CommSpec comm_spec_;

  MPI_Comm comm_;

  std::vector<InArchive> to_self_;
  std::vector<InArchive> to_others_;

  std::vector<ThreadLocalMessageBuffer<ParallelMessageManager>> channels_;
  int round_;
  bool terminate_;

  BlockingQueue<std::pair<fid_t, InArchive>> sending_queue_;
  std::thread send_thread_;

  BlockingQueue<OutArchive> recv_queues_;
  std::thread recv_thread_;
  std::thread termcheck_thread_;

  bool force_continue_;
  size_t sent_size_;
};

}  // namespace grape

#endif  // GRAPE_PARALLEL_PARALLEL_MESSAGE_MANAGER_H_
