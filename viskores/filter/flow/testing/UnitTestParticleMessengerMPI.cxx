//============================================================================
//  The contents of this file are covered by the Viskores license. See
//  LICENSE.txt for details.
//
//  By contributing to this file, all contributors agree to the Developer
//  Certificate of Origin Version 1.1 (DCO 1.1) as stated in DCO.txt.
//============================================================================

//============================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//============================================================================

#include <viskores/cont/Serialization.h>
#include <viskores/cont/testing/Testing.h>
#include <viskores/filter/flow/internal/ParticleMessenger.h>

#include <viskores/thirdparty/diy/diy.h>

#include <random>

namespace
{
using PCommType = std::pair<viskores::Particle, std::vector<viskores::Id>>;
using MCommType = std::pair<int, std::vector<int>>;
using PCommType = std::pair<viskores::Particle, std::vector<viskores::Id>>;
using PRecvCommType = std::pair<int, std::vector<PCommType>>;

class TestMessenger : public viskores::filter::flow::internal::ParticleMessenger<viskores::Particle>
{
public:
  TestMessenger(viskoresdiy::mpi::communicator& comm,
                bool useAsyncComm,
                const viskores::filter::flow::internal::BoundsMap& bm,
                int msgSz = 1,
                int numParticles = 1,
                int numBlockIds = 1)
    : ParticleMessenger(comm, useAsyncComm, bm, msgSz, numParticles, numBlockIds)
  {
  }

  void GetBufferSizes(int numParticles,
                      int numBlockIds,
                      int msgSz,
                      std::size_t& pBuffSize,
                      std::size_t& mBuffSize)
  {
    pBuffSize = this->CalcParticleBufferSize(numParticles, numBlockIds);
    mBuffSize = this->CalcMessageBufferSize(msgSz);
  }

  void SendP(int dst,
             const std::vector<viskores::Particle>& p,
             const std::vector<std::vector<viskores::Id>>& bids)
  {
    std::vector<PCommType> data;
    for (std::size_t i = 0; i < p.size(); i++)
      data.push_back(PCommType(p[i], bids[i]));

    this->SendParticles(dst, data);
  }

  void SendM(int dst, const std::vector<int>& msg) { this->SendMsg(dst, { msg }); }

  void SendMAll(int msg) { this->SendAllMsg({ msg }); }

  bool ReceiveAnything(std::vector<MCommType>* msgs,
                       std::vector<PRecvCommType>* recvParticles,
                       bool blockAndWait = false)
  {
    return this->RecvAny(msgs, recvParticles, blockAndWait);
  }
};

void ValidateReceivedParticles(
  int sendRank,
  const std::vector<PCommType>& recvP,
  const std::vector<std::vector<viskores::Particle>>& particles,
  const std::vector<std::vector<std::vector<viskores::Id>>>& particleBlockIds)
{
  //Make sure the right number of particles were received from sender.
  std::size_t numReqParticles = particles[static_cast<std::size_t>(sendRank)].size();
  std::size_t numRecvParticles = recvP.size();
  VISKORES_TEST_ASSERT(numReqParticles == numRecvParticles, "Wrong number of particles received.");

  //Make sure each particle is the same.
  for (std::size_t i = 0; i < numRecvParticles; i++)
  {
    const auto& reqP = particles[static_cast<std::size_t>(sendRank)][i];
    const auto& p = recvP[i].first;

    VISKORES_TEST_ASSERT(p.GetPosition() == reqP.GetPosition(),
                         "Received particle has wrong Position.");
    VISKORES_TEST_ASSERT(p.GetTime() == reqP.GetTime(), "Received particle has wrong Time.");
    VISKORES_TEST_ASSERT(p.GetID() == reqP.GetID(), "Received particle has wrong ID.");
    VISKORES_TEST_ASSERT(p.GetNumberOfSteps() == reqP.GetNumberOfSteps(),
                         "Received particle has wrong NumSteps.");

    VISKORES_TEST_ASSERT(p.GetPosition() == reqP.GetPosition() && p.GetTime() == reqP.GetTime() &&
                           p.GetID() == reqP.GetID() &&
                           p.GetNumberOfSteps() == reqP.GetNumberOfSteps(),
                         "Received particle has wrong values.");

    const auto& reqBids = particleBlockIds[static_cast<std::size_t>(sendRank)][i];
    const auto& bids = recvP[i].second;
    VISKORES_TEST_ASSERT(reqBids.size() == bids.size(), "Wrong number of particle block ids.");
    for (std::size_t j = 0; j < bids.size(); j++)
      VISKORES_TEST_ASSERT(bids[j] == reqBids[j], "Wrong block Id.");
  }
}

void ValidateReceivedMessage(int sendRank,
                             const std::vector<int>& recvMsg,
                             const std::vector<std::vector<int>>& messages)
{
  const auto& reqMsg = messages[static_cast<std::size_t>(sendRank)];
  VISKORES_TEST_ASSERT(recvMsg.size() == reqMsg.size(), "Wrong message size received");

  for (std::size_t i = 0; i < recvMsg.size(); i++)
    VISKORES_TEST_ASSERT(reqMsg[i] == recvMsg[i], "Wrong message value received");
}

void TestParticleMessenger()
{
  auto comm = viskores::cont::EnvironmentTracker::GetCommunicator();

  //Only works for 2 or more ranks.
  if (comm.size() == 1)
    return;
  viskores::filter::flow::internal::BoundsMap boundsMap;

  int maxMsgSz = 100;
  int maxNumParticles = 128;
  int maxNumBlockIds = 5 * comm.size();
  TestMessenger messenger(
    comm, true, boundsMap, maxMsgSz / 2, maxNumParticles / 2, maxNumBlockIds / 2);

  //create some data.
  std::vector<std::vector<viskores::Particle>> particles(comm.size());
  std::vector<std::vector<std::vector<viskores::Id>>> particleBlockIds(comm.size());
  std::vector<std::vector<int>> messages(comm.size());

  std::random_device device;
  std::default_random_engine generator(static_cast<viskores::UInt32>(83921));
  viskores::FloatDefault v0(-100), v1(100);
  std::uniform_real_distribution<viskores::FloatDefault> floatDist(v0, v1);
  std::uniform_int_distribution<int> idDist(0, 10000), bidDist(1, maxNumBlockIds);
  std::uniform_int_distribution<int> nPDist(1, maxNumParticles), nStepsDist(10, 100);
  std::uniform_int_distribution<int> msgSzDist(1, maxMsgSz);

  //initialize particles and messages.
  std::size_t rank = static_cast<std::size_t>(comm.rank());
  std::size_t numRanks = static_cast<std::size_t>(comm.size());

  viskores::Id pid = 0;
  for (std::size_t r = 0; r < numRanks; r++)
  {
    int nP = nPDist(generator);
    std::vector<viskores::Particle> pvec;
    std::vector<std::vector<viskores::Id>> blockIds;
    for (int p = 0; p < nP; p++)
    {
      viskores::Particle particle;
      particle.SetPosition({ floatDist(generator), floatDist(generator), floatDist(generator) });
      particle.SetTime(floatDist(generator));
      particle.SetID(pid++);
      particle.SetNumberOfSteps(nStepsDist(generator));
      pvec.push_back(particle);

      std::vector<viskores::Id> bids(bidDist(generator));
      for (auto& b : bids)
        b = static_cast<viskores::Id>(idDist(generator));
      blockIds.push_back(bids);
    }

    //set the particles for this rank.
    particles[r] = pvec;
    particleBlockIds[r] = blockIds;

    //set the messages for this rank.
    std::vector<int> msg(msgSzDist(generator));
    for (auto& m : msg)
      m = idDist(generator);
    messages[r] = msg;
  }

  bool done = false;

  std::uniform_int_distribution<int> rankDist(0, comm.size() - 1);
  int particleRecvCtr = 0, msgRecvCtr = 0;

  constexpr int DONE_MSG = -100;

  while (!done)
  {
    int dst = rankDist(generator);
    if (dst != comm.rank())
    {
      std::vector<viskores::Particle> sendP = particles[rank];
      std::vector<std::vector<viskores::Id>> sendIds = particleBlockIds[rank];
      messenger.SendP(dst, sendP, sendIds);
    }

    dst = rankDist(generator);
    if (dst != comm.rank())
      messenger.SendM(dst, messages[rank]);

    std::vector<MCommType> msgData;
    std::vector<PRecvCommType> particleData;
    if (messenger.ReceiveAnything(&msgData, &particleData))
    {
      if (!msgData.empty())
        msgRecvCtr++;
      if (!particleData.empty())
        particleRecvCtr++;

      //Validate what we received.
      for (const auto& md : msgData)
      {
        int sendRank = md.first;
        const auto& recvM = md.second;

        //check for done message.
        if (sendRank == 0 && recvM.size() == 1 && recvM[0] == DONE_MSG)
          done = true;
        else
          ValidateReceivedMessage(sendRank, recvM, messages);
      }

      for (const auto& pd : particleData)
      {
        int sendRank = pd.first;
        const auto& recvP = pd.second;
        ValidateReceivedParticles(sendRank, recvP, particles, particleBlockIds);
      }

      //We are done once rank0 receives at least 25 messages and particles.
      if (rank == 0 && msgRecvCtr > 25 && particleRecvCtr > 25)
      {
        done = true;
        messenger.SendMAll(DONE_MSG);
      }
    }
  }

  comm.barrier();
}

void TestBufferSizes()
{
  //Make sure the buffer sizes are correct.
  auto comm = viskores::cont::EnvironmentTracker::GetCommunicator();
  viskores::filter::flow::internal::BoundsMap boundsMap;

  std::vector<int> mSzs = { 1, 2, 3, 4, 5 };
  std::vector<int> numPs = { 1, 2, 3, 4, 5 };
  std::vector<int> numBids = { 0, 1, 2, 3, 4, 5 };

  for (const auto& mSz : mSzs)
    for (const auto& numP : numPs)
      for (const auto& nBids : numBids)
      {
        TestMessenger messenger(comm, true, boundsMap);

        std::size_t pSize, mSize;
        messenger.GetBufferSizes(numP, nBids, mSz, pSize, mSize);

        //Make sure message buffers are the right size.
        int rank = 0;
        viskoresdiy::MemoryBuffer mbM;
        std::vector<int> msg(mSz);
        viskoresdiy::save(mbM, rank);
        viskoresdiy::save(mbM, msg);
        VISKORES_TEST_ASSERT(mbM.size() == mSize, "Message buffer sizes not equal");

        //Make sure particle buffers are the right size.
        std::vector<PCommType> particleData;
        for (int i = 0; i < numP; i++)
        {
          viskores::Particle p;
          std::vector<viskores::Id> bids(nBids, 0);
          particleData.push_back(std::make_pair(p, bids));
        }

        viskoresdiy::MemoryBuffer mbP;
        viskoresdiy::save(mbP, rank);
        viskoresdiy::save(mbP, particleData);
        VISKORES_TEST_ASSERT(mbP.size() == pSize, "Particle buffer sizes not equal");
      }
}

void TestParticleMessengerMPI()
{
  TestBufferSizes();
  TestParticleMessenger();
}
}

int UnitTestParticleMessengerMPI(int argc, char* argv[])
{
  return viskores::cont::testing::Testing::Run(TestParticleMessengerMPI, argc, argv);
}
