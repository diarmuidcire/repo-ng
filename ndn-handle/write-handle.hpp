/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (C) 2014 Regents of the University of California.
 * See COPYING for copyright and distribution information.
 */

#ifndef REPO_NDN_HANDLE_WRITE_HANDLE_HPP
#define REPO_NDN_HANDLE_WRITE_HANDLE_HPP

#include "ndn-handle-common.hpp"
#include "base-handle.hpp"
#include <queue>

namespace repo {

using std::map;
using std::pair;
using std::queue;

/**
* @brief WriteHandle provides basic credit based congestion control.
*
* First repo sends interests of credit number and then credit will be 0.
*
* If a data comes, credit++ and sends a interest then credit--.
*
* If the interest timeout, repo will retry and send interest in retrytimes.
*
* If one interest timeout beyond retrytimes, the fetching process will terminate.
*
* Another case is that if command will insert segmented data without EndBlockId.
*
* The repo will keep fetching data in noendTimeout time.
*
* If data returns with FinalBlockId, this detecting timeout process will terminate.
*
* If client sends a insert check command, the noendTimeout timer will be set to 0.
*
* If repo cannot get FinalBlockId in noendTimeout time, the fetching process will terminate.
*/
class WriteHandle : public BaseHandle
{

public:
  class Error : public BaseHandle::Error
  {
  public:
    explicit
    Error(const std::string& what)
      : BaseHandle::Error(what)
    {
    }
  };


public:
  WriteHandle(Face& face, StorageHandle& storageHandle, KeyChain& keyChain,
              Scheduler& scheduler, CommandInterestValidator& validator);

  virtual void
  listen(const Name& prefix);

private:
  /**
  * @brief Information of insert process including variables for response
  *        and credit based flow control
  */
  struct ProcessInfo
  {
    //ProcessId id;
    RepoCommandResponse response;
    queue<SegmentNo> nextSegmentQueue;  ///< queue of waiting segment
                                        ///  to be sent when having credits
    SegmentNo nextSegment;  ///< last segment put into the nextSegmentQueue
    map<SegmentNo, int> retryCounts;  ///< to store retrying times of timeout segment
    int credit;  ///< congestion control credits of process

    /**
     * @brief the latest time point at which EndBlockId must be determined
     *
     * Segmented fetch process will terminate if EndBlockId cannot be
     * determined before this time point.
     * It is initialized to now()+noEndTimeout when segmented fetch process begins,
     * and reset to now()+noEndTimeout each time an insert status check command is processed.
     */
    ndn::time::steady_clock::TimePoint noEndTime;
  };

private: // insert command
  /**
   * @brief handle insert commands
   */
  void
  onInterest(const Name& prefix, const Interest& interest);

  void
  onValidated(const shared_ptr<const Interest>& interest, const Name& prefix);

  void
  onValidationFailed(const shared_ptr<const Interest>& interest);

  /**
   * @brief insert command prefix register failed
   */
  void
  onRegisterFailed(const Name& prefix, const std::string& reason);

private: // single data fetching
  /**
   * @brief fetch one data
   */
  void
  onData(const Interest& interest, Data& data, ProcessId processId);

  /**
   * @brief handle when fetching one data timeout
   */
  void
  onTimeout(const Interest& interest, ProcessId processId);

  void
  processSingleInsertCommand(const Interest& interest, RepoCommandParameter& parameter);

private:  // segmented data fetching
  /**
   * @brief fetch segmented data
   */
  void
  onSegmentData(const Interest& interest, Data& data, ProcessId processId);

  /**
   * @brief Timeout when fetching segmented data. Data can be fetched RETRY_TIMEOUT times.
   */
  void
  onSegmentTimeout(const Interest& interest, ProcessId processId);

  /**
   * @brief initiate fetching segmented data
   */
  void
  segInit(ProcessId processId, const RepoCommandParameter& parameter);

  /**
   * @brief control for sending interests in function onSegmentData()
   */
  void
  onSegmentDataControl(ProcessId processId, const Interest& interest);

  /**
   * @brief control for sending interest in function onSegmentTimeout
   */
  void
  onSegmentTimeoutControl(ProcessId processId, const Interest& interest);

  void
  processSegmentedInsertCommand(const Interest& interest, RepoCommandParameter& parameter);

  /**
   * @brief extends noEndTime of process if not noEndTimeout, set StatusCode 405
   *
   * called by onCheckValidated() if there is no EndBlockId. If not noEndTimeout,
   * extends noEndTime of process. If noEndTimeout, set status code 405 as noEndTimeout.
   */
  void
  extendNoEndTime(ProcessInfo& process);

private: // insert state check command
  /**
   * @brief handle insert check command
   */
  void
  onCheckInterest(const Name& prefix, const Interest& interest);

  /**
   * @brief insert check command prefix register failed
   */
  void
  onCheckRegisterFailed(const Name& prefix, const std::string& reason);

  void
  onCheckValidated(const shared_ptr<const Interest>& interest, const Name& prefix);

  void
  onCheckValidationFailed(const shared_ptr<const Interest>& interest);

private:
  void
  deleteProcess(ProcessId processId);

  /**
   * @brief schedule a event to delete the process
   */
  void
  deferredDeleteProcess(ProcessId processId);

  void
  negativeReply(const Interest& interest, int statusCode);

private:

  CommandInterestValidator& m_validator;

  map<ProcessId, ProcessInfo> m_processes;

  int m_retryTime;
  int m_credit;
  ndn::time::milliseconds m_noEndTimeout;
};

} // namespace repo

#endif // REPO_NDN_HANDLE_WRITE_HANDLE_HPP