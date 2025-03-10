// Copyright 2019-2020 CERN and copyright holders of ALICE O2.
// See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
// All rights not expressly granted are reserved.
//
// This software is distributed under the terms of the GNU General Public
// License v3 (GPL Version 3), copied verbatim in the file "COPYING".
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
///
/// \brief
///   ATTENTION Nov. 2021:
///   MFT is not implemented yet and can not be used
///   releated code is commented
/// \author Paul Buehler, paul.buehler@oeaw.ac.at
/// \since  01.10.2021

#include "Framework/ConfigParamSpec.h"

using namespace o2;
using namespace o2::framework;

void customize(std::vector<ConfigParamSpec>& workflowOptions)
{
  workflowOptions.push_back(ConfigParamSpec{"runCase", VariantType::Int, 0, {"runCase: 0 - histos,  1 - mcTruth, else - tree"}});
}

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"

#include "PWGUD/DataModel/McPIDTable.h"

#include "CommonConstants/LHCConstants.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/Core/PID/PIDResponse.h"
#include "Common/Core/MC.h"

using namespace o2::framework::expressions;

template <typename T>
T getCompatibleBCs(soa::Join<aod::Collisions, aod::EvSels, aod::McCollisionLabels>::iterator const& collision, T const& bcs)
{
  LOGF(debug, "Collision time / resolution [ns]: %f / %f", collision.collisionTime(), collision.collisionTimeRes());

  auto bcIter = collision.bc_as<T>();

  // due to the filling scheme the most probably BC may not be the one estimated from the collision time
  uint64_t mostProbableBC = bcIter.globalBC();
  uint64_t meanBC = mostProbableBC - std::lround(collision.collisionTime() / o2::constants::lhc::LHCBunchSpacingNS);
  int deltaBC = std::ceil(collision.collisionTimeRes() / o2::constants::lhc::LHCBunchSpacingNS * 4);
  int64_t minBC = meanBC - deltaBC;
  uint64_t maxBC = meanBC + deltaBC;
  if (minBC < 0) {
    minBC = 0;
  }

  // find slice of BCs table with BC in [minBC, maxBC]
  int64_t maxBCId = bcIter.globalIndex();
  int moveCount = 0; // optimize to avoid to re-create the iterator
  while (bcIter != bcs.end() && bcIter.globalBC() <= maxBC && (int64_t)bcIter.globalBC() >= minBC) {
    LOGF(debug, "Table id %d BC %llu", bcIter.globalIndex(), bcIter.globalBC());
    maxBCId = bcIter.globalIndex();
    ++bcIter;
    ++moveCount;
  }

  bcIter.moveByIndex(-moveCount); // Move back to original position
  int64_t minBCId = collision.bcId();
  while (bcIter != bcs.begin() && bcIter.globalBC() <= maxBC && (int64_t)bcIter.globalBC() >= minBC) {
    LOGF(debug, "Table id %d BC %llu", bcIter.globalIndex(), bcIter.globalBC());
    minBCId = bcIter.globalIndex();
    --bcIter;
  }

  LOGF(debug, "  BC range: %i (%d) - %i (%d)", minBC, minBCId, maxBC, maxBCId);

  T slice{{bcs.asArrowTable()->Slice(minBCId, maxBCId - minBCId + 1)}, (uint64_t)minBCId};
  bcs.copyIndexBindings(slice);
  return slice;
}

// Loop over collisions
// find for each collision the number of compatible BCs
struct CompatibleBCs {
  using CCs = soa::Join<aod::Collisions, aod::EvSels, aod::McCollisionLabels>;
  using CC = CCs::iterator;

  void process(CC const& collision, aod::BCs const& bcs)
  {
    auto bcSlice = getCompatibleBCs(collision, bcs);
    LOGF(debug, "  Number of possible BCs: %i", bcSlice.size());
    for (auto& bc : bcSlice) {
      LOGF(debug, "    This collision may belong to BC %lld", bc.globalBC());
    }
  }
};

// Loop over collisions
// Fill histograms with collision and compatible BCs related information
struct collisionsInfo {
  int cnt = 0;
  HistogramRegistry registry{
    "registry",
    {
      {"timeResolution", "#timeResolution", {HistType::kTH1F, {{200, 0., 1.E3}}}},
      {"numberBCs", "#numberBCs", {HistType::kTH1F, {{101, -0.5, 100.5}}}},
      {"DGCandidate", "#DGCandidate", {HistType::kTH1F, {{2, -0.5, 1.5}}}},
      {"numberTracks", "#numberTracks", {HistType::kTH1F, {{301, -0.5, 300.5}}}},
      {"numberVtxTracks", "#numberVtxTracks", {HistType::kTH1F, {{101, -0.5, 100.5}}}},
      {"numberGlobalTracks", "#numberGlobalTracks", {HistType::kTH1F, {{101, -0.5, 100.5}}}},
      {"netCharge", "#netCharge", {HistType::kTH1F, {{3, -1.5, 1.5}}}},
      //      {"numberMFTTracks", "#numberMFTTracks", {HistType::kTH1F, {{101, -0.5, 100.5}}}},
      //      {"etaMFTAll", "#etaMFTAll", {HistType::kTH1F, {{100, -5.0, 5.0}}}},
      //      {"etaMFTDG", "#etaMFTDG", {HistType::kTH1F, {{100, -5.0, 5.0}}}},
      {"numberFWDTracks", "#numberFWDTracks", {HistType::kTH1F, {{101, -0.5, 100.5}}}},
      {"etaFWDAll", "#etaFWDAll", {HistType::kTH1F, {{100, -5.0, 5.0}}}},
      {"etaFWDDG", "#etaFWDDG", {HistType::kTH1F, {{100, -5.0, 5.0}}}},
      //      {"globalVsMFTAll", "#globalVsMFTAll", {HistType::kTH2F, {{21, -0.5, 20.5}, {21, -0.5, 20.5}}}},
      //      {"globalVsMFTDG", "#globalVsMFTDG", {HistType::kTH2F, {{21, -0.5, 20.5}, {21, -0.5, 20.5}}}},
    }};

  void init(o2::framework::InitContext&)
  {
    registry.get<TH1>(HIST("timeResolution"))->GetXaxis()->SetTitle("Time resolution [ns]");
    registry.get<TH1>(HIST("numberBCs"))->GetXaxis()->SetTitle("Number of compatible BCs");
    registry.get<TH1>(HIST("numberTracks"))->GetXaxis()->SetTitle("Number of tracks");
    registry.get<TH1>(HIST("numberVtxTracks"))->GetXaxis()->SetTitle("Number of Vtx tracks");
    registry.get<TH1>(HIST("numberGlobalTracks"))->GetXaxis()->SetTitle("Number of global tracks");
    registry.get<TH1>(HIST("netCharge"))->GetXaxis()->SetTitle("Sign of net charge");
    //    registry.get<TH1>(HIST("numberMFTTracks"))->GetXaxis()->SetTitle("Number of MFT tracks");
    //    registry.get<TH1>(HIST("etaMFTAll"))->GetXaxis()->SetTitle("Pseudo rapidity");
    //    registry.get<TH1>(HIST("etaMFTDG"))->GetXaxis()->SetTitle("Pseudo rapidity");
    registry.get<TH1>(HIST("numberFWDTracks"))->GetXaxis()->SetTitle("Number of FWD tracks");
    registry.get<TH1>(HIST("etaFWDAll"))->GetXaxis()->SetTitle("Pseudo rapidity");
    registry.get<TH1>(HIST("etaFWDDG"))->GetXaxis()->SetTitle("Pseudo rapidity");
    //    registry.get<TH2>(HIST("globalVsMFTAll"))->GetXaxis()->SetTitle("Number of global tracks");
    //    registry.get<TH2>(HIST("globalVsMFTAll"))->GetYaxis()->SetTitle("Number of MFT tracks");
    //    registry.get<TH2>(HIST("globalVsMFTDG"))->GetXaxis()->SetTitle("Number of global tracks");
    //    registry.get<TH2>(HIST("globalVsMFTDG"))->GetYaxis()->SetTitle("Number of MFT tracks");
  }

  using CCs = soa::Join<aod::Collisions, aod::EvSels, aod::McCollisionLabels>;
  using CC = CCs::iterator;
  using BCs = soa::Join<aod::BCs, aod::Run3MatchedToBCSparse>;
  using TCs = soa::Join<aod::Tracks, aod::TrackSelection>;
  using MFs = aod::MFTTracks;
  using FWs = aod::FwdTracks;

  void process(CC const& collision, BCs const& bct0s,
               TCs& tracks, /* MFs& mfttracks,*/ FWs& fwdtracks, aod::FT0s& ft0s, aod::FV0As& fv0as, aod::FDDs& fdds,
               aod::McCollisions& McCols, aod::McParticles& McParts)
  {

    // obtain slice of compatible BCs
    auto bcSlice = getCompatibleBCs(collision, bct0s);
    LOGF(debug, "  Number of compatible BCs: %i", bcSlice.size());
    registry.get<TH1>(HIST("numberBCs"))->Fill(bcSlice.size());

    // check that there are no FIT signals in any of the compatible BCs
    auto isDGcandidate = true;
    for (auto& bc : bcSlice) {
      if (bc.has_ft0() || bc.has_fv0a() || bc.has_fdd()) {
        isDGcandidate = false;
        break;
      }
    }
    // count tracks
    int cntAll = 0;
    int cntGlobal = 0;
    int netCharge = 0;
    for (auto track : tracks) {
      cntAll++;
      netCharge += track.sign();
      if (track.isGlobalTrack()) {
        cntGlobal++;
      }
    }
    // netCharge /= abs(netCharge);

    if (isDGcandidate) {
      LOGF(info, "  This is a DG candidate with %d tracks and %d net charge.", tracks.size(), netCharge);
      registry.get<TH1>(HIST("DGCandidate"))->Fill(1.);
    } else {
      registry.get<TH1>(HIST("DGCandidate"))->Fill(0.);
    }

    // update histograms with track information
    LOGF(debug, "Number of tracks: Vertex %d, total %d, global %d", collision.numContrib(), cntAll, cntGlobal);
    LOGF(debug, "Number of SPD cluster: %d", collision.spdClusters());
    registry.get<TH1>(HIST("numberTracks"))->Fill(cntAll);
    registry.get<TH1>(HIST("numberVtxTracks"))->Fill(collision.numContrib());
    registry.get<TH1>(HIST("numberGlobalTracks"))->Fill(cntGlobal);
    registry.get<TH1>(HIST("netCharge"))->Fill(netCharge);
    //    registry.get<TH1>(HIST("numberMFTTracks"))->Fill(mfttracks.size());
    registry.get<TH1>(HIST("numberFWDTracks"))->Fill(fwdtracks.size());
    //    registry.get<TH2>(HIST("globalVsMFTAll"))->Fill(cntGlobal, mfttracks.size());

    // loop over MFT tracks
    //    LOGF(debug, "MFT tracks: %i", mfttracks.size());
    //    for (auto mfttrack : mfttracks) {
    //      registry.get<TH1>(HIST("etaMFTAll"))->Fill(mfttrack.eta());
    //    }

    // loop over FWD tracks
    LOGF(info, "FWD tracks: %i", fwdtracks.size());
    for (auto fwdtrack : fwdtracks) {
      registry.get<TH1>(HIST("etaFWDAll"))->Fill(fwdtrack.eta());
    }

    // update timeResolution
    registry.get<TH1>(HIST("timeResolution"))->Fill(collision.collisionTimeRes());

    if (isDGcandidate) {
      // loop over MFT tracks
      //      for (auto mfttrack : mfttracks) {
      //        registry.get<TH1>(HIST("etaMFTDG"))->Fill(mfttrack.eta());
      //      }
      //      registry.get<TH2>(HIST("globalVsMFTDG"))->Fill(cntGlobal, mfttracks.size());

      // loop over FWD tracks
      for (auto fwdtrack : fwdtracks) {
        registry.get<TH1>(HIST("etaFWDDG"))->Fill(fwdtrack.eta());
      }
    }
    cnt++;
    LOGF(debug, "#Collisions: %d", cnt);
  }
};

// Loop over BCs
// check aliases, selection, and FIT signals per BC
struct BCInfo {
  int cnt = 0;
  HistogramRegistry registry{
    "registry",
    {{"numberCollisions", "#numberCollisions", {HistType::kTH1F, {{11, -0.5, 10.5}}}},
     {"numberCollisionsGT", "#numberCollisionsGT", {HistType::kTH1F, {{11, -0.5, 10.5}}}},
     {"Aliases", "#Aliases", {HistType::kTH1F, {{kNaliases, 0., kNaliases}}}},
     {"Selection", "#Selection", {HistType::kTH1F, {{aod::kNsel, 0., aod::kNsel}}}},
     {"DetectorSignals", "#DetectorSignals", {HistType::kTH1F, {{6, 0., 6}}}}}};

  void init(o2::framework::InitContext&)
  {
    registry.get<TH1>(HIST("numberCollisions"))->GetXaxis()->SetTitle("#Collisions per BC");
    registry.get<TH1>(HIST("numberCollisionsGT"))->GetXaxis()->SetTitle("#Collisions with good time per BC");
  }

  using BBs = soa::Join<aod::BCs, aod::BcSels, aod::Run3MatchedToBCSparse>;
  using BB = BBs::iterator;

  void process(BB const& bc, aod::Collisions const& cols)
  {
    LOGF(debug, "BC: %d number of collisions: %d", bc.globalBC(), cols.size());
    registry.get<TH1>(HIST("numberCollisions"))->Fill(cols.size());

    // count collisions with good time resoluton
    auto nColGT = 0;
    for (auto col : cols) {
      if (col.collisionTimeRes() <= 20.) {
        nColGT++;
      }
    }
    registry.get<TH1>(HIST("numberCollisionsGT"))->Fill(nColGT);

    // update Aliases
    auto aliases = bc.alias();
    for (auto ii = 0; ii < kNaliases; ii++) {
      registry.get<TH1>(HIST("Aliases"))->Fill(ii, aliases[ii]);
    }

    // update Selection
    auto selections = bc.selection();
    for (auto ii = 0; ii < aod::kNsel; ii++) {
      registry.get<TH1>(HIST("Selection"))->Fill(ii, selections[ii]);
    }

    // FIT detector signals
    if (!bc.has_ft0()) {
      registry.get<TH1>(HIST("DetectorSignals"))->Fill(0., 1.);
    }
    if (!bc.has_fv0a()) {
      registry.get<TH1>(HIST("DetectorSignals"))->Fill(1., 1.);
    }
    if (!bc.has_fdd()) {
      registry.get<TH1>(HIST("DetectorSignals"))->Fill(2., 1.);
    }
    if (!bc.has_zdc()) {
      registry.get<TH1>(HIST("DetectorSignals"))->Fill(3., 1.);
    }
    auto noFIT = !bc.has_fv0a() && !bc.has_fv0a() && !bc.has_fdd();
    if (noFIT) {
      registry.get<TH1>(HIST("DetectorSignals"))->Fill(4., 1.);
    }
    if (noFIT && !bc.has_zdc()) {
      registry.get<TH1>(HIST("DetectorSignals"))->Fill(5., 1.);
    }
    cnt++;
    LOGF(debug, "#BCs: %d", cnt);
  }
};

// Loop over tracks
// Make histograms with track type and time resolution
struct TrackTypes {
  HistogramRegistry registry{
    "registry",
    {
      {"nTracks", "#nTracks", {HistType::kTH2F, {{6, -0.5, 5.5}, {2, 0., 2.}}}},
      {"timeRes", "#timeRes", {HistType::kTH2F, {{6, -0.5, 5.5}, {2, 0., 2.}}}},
      {"FwdType", "#FwdType", {HistType::kTH2F, {{7, -0.5, 6.5}, {1, -0.5, 0.5}}}},
    }};

  using TCs = soa::Join<aod::Tracks, aod::TracksExtra, aod::TrackSelection>;
  using MTs = aod::MFTTracks;
  using FTs = aod::FwdTracks;

  void process(TCs const& tracks, /*MTs const& mfttracks,*/ FTs const& fwdtracks)
  {
    for (auto track : tracks) {
      LOGF(debug, "isGlobal %i Detector map %i %i %i %i time resolution %f", track.isGlobalTrack(),
           track.hasITS(), track.hasTPC(), track.hasTRD(), track.hasTOF(), track.trackTimeRes());

      float isGlobal = track.isGlobalTrack() ? 1. : 0.;

      registry.get<TH2>(HIST("nTracks"))->Fill(0., isGlobal, 1.);
      registry.get<TH2>(HIST("timeRes"))->Fill(0., isGlobal, track.trackTimeRes());

      // has associated collision
      if (track.collisionId() >= 0) {
        registry.get<TH2>(HIST("nTracks"))->Fill(1., isGlobal, 1.);
        registry.get<TH2>(HIST("timeRes"))->Fill(1., isGlobal, track.trackTimeRes());
      }

      // has ITS hit
      if (track.hasITS()) {
        registry.get<TH2>(HIST("nTracks"))->Fill(2., isGlobal, 1.);
        registry.get<TH2>(HIST("timeRes"))->Fill(2., isGlobal, track.trackTimeRes());
      }

      // has TPC hit
      if (track.hasTPC()) {
        registry.get<TH2>(HIST("nTracks"))->Fill(3., isGlobal, 1.);
        registry.get<TH2>(HIST("timeRes"))->Fill(3., isGlobal, track.trackTimeRes());
      }

      // has TRD hit
      if (track.hasTRD()) {
        registry.get<TH2>(HIST("nTracks"))->Fill(4., isGlobal, 1.);
        registry.get<TH2>(HIST("timeRes"))->Fill(4., isGlobal, track.trackTimeRes());
      }

      // has TOF hit
      if (track.hasTOF()) {
        registry.get<TH2>(HIST("nTracks"))->Fill(5., isGlobal, 1.);
        registry.get<TH2>(HIST("timeRes"))->Fill(5., isGlobal, track.trackTimeRes());
      }
    }

    // ForwardTrackTypeEnum has 5 values
    auto nTypes = 5;
    for (auto fwdtrack : fwdtracks) {
      registry.get<TH2>(HIST("FwdType"))->Fill(0., 0., 1.);
      if (fwdtrack.collisionId() >= 0) {
        registry.get<TH2>(HIST("FwdType"))->Fill(1., 0., 1.);
      }
      for (auto ii = 0; ii < nTypes; ii++) {
        if ((fwdtrack.trackType() & (1 << ii)) > 0) {
          registry.get<TH2>(HIST("FwdType"))->Fill((Double_t)(ii + 2), 0., 1.);
        }
      }
    }
  }
};

// MCTruth tracks
struct MCTracks {

  using CCs = soa::Join<aod::Collisions, aod::McCollisionLabels>;
  using CC = CCs::iterator;

  void process(CCs const& collisions, aod::McCollisions& McCols, aod::McParticles& McParts)
  {

    for (auto collision : collisions) {

      // get McCollision which belongs to collision
      auto MCCol = collision.mcCollision();

      LOGF(info, "Collision %i MC collision %i",
           collision.globalIndex(), MCCol.globalIndex());

      // get MCParticles which belong to MCCol
      auto MCPartSlice = McParts.sliceBy(aod::mcparticle::mcCollisionId, MCCol.globalIndex());
      LOGF(info, "  Number of McParticles %i", MCPartSlice.size());

      // loop over particles
      float etot = 0.0;
      float px = 0.0, py = 0.0, pz = 0.0;
      bool hasDiff = false;
      int prongs = 0;

      for (auto mcpart : MCPartSlice) {
        LOGF(info, " MCPart: %i %i %i %i %i - %i %i %i", mcpart.mcCollisionId(), mcpart.isPhysicalPrimary(), mcpart.getProcess(), mcpart.getGenStatusCode(), mcpart.globalIndex(), mcpart.pdgCode(), mcpart.mother0Id(), mcpart.mother1Id());
        if (mcpart.pdgCode() == 9900110) {
          LOGF(info, "  rho_diff0 energy: %f", mcpart.e());
          hasDiff = true;
        }

        if (hasDiff) {
          if (mcpart.isPhysicalPrimary() &&
              (mcpart.getGenStatusCode() == 1 || mcpart.getGenStatusCode() == 2) &&
              mcpart.mother0Id() != mcpart.mother1Id() &&
              mcpart.mother1Id() > 0) {

            prongs++;
            etot += mcpart.e();
            px += mcpart.px();
            py += mcpart.py();
            pz += mcpart.pz();
          }
        }
      }
      if (hasDiff) {
        auto mass = TMath::Sqrt(etot * etot - (px * px + py * py + pz * pz));
        LOGF(info, "  mass of X: %f, prongs: %i", mass, prongs);
      }
    }
  }
};

// TPC nSigma
struct TPCnSigma {
  Produces<aod::UDnSigmas> nSigmas;

  using TCs = soa::Join<aod::Tracks, aod::TrackSelection, aod::McTrackLabels>;
  using TCwPIDs = soa::Join<TCs, aod::pidTPCEl, aod::pidTPCMu, aod::pidTPCPi, aod::pidTPCKa, aod::pidTPCPr>;

  void process(TCwPIDs& tracks, aod::McParticles const& mcParticles)
  {
    for (auto track : tracks) {
      if (track.isGlobalTrack()) {
        nSigmas(track.mcParticle().pdgCode(), track.mcParticle().pt(),
                track.tpcNSigmaEl(), track.tpcNSigmaMu(), track.tpcNSigmaPi(),
                track.tpcNSigmaKa(), track.tpcNSigmaPr());
      }
    }
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  if (cfgc.options().get<int>("runCase") == 0) {
    return WorkflowSpec{
      // adaptAnalysisTask<CompatibleBCs>(cfgc),
      adaptAnalysisTask<collisionsInfo>(cfgc, TaskName{"collisioninformation"}),
      adaptAnalysisTask<BCInfo>(cfgc, TaskName{"bcinformation"}),
      adaptAnalysisTask<TrackTypes>(cfgc, TaskName{"tracktypes"}),
    };
  } else if (cfgc.options().get<int>("runCase") == 1) {
    return WorkflowSpec{
      adaptAnalysisTask<MCTracks>(cfgc, TaskName{"mctracks"}),
    };
  } else {
    return WorkflowSpec{
      adaptAnalysisTask<TPCnSigma>(cfgc, TaskName{"tpcnsigma"}),
    };
  }
}
