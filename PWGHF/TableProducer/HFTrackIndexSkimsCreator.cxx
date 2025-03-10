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

/// \file HFTrackIndexSkimsCreator.cxx
/// \brief Pre-selection of 2-prong and 3-prong secondary vertices of heavy-flavour decay candidates
///
/// \author Gian Michele Innocenti <gian.michele.innocenti@cern.ch>, CERN
/// \author Vít Kučera <vit.kucera@cern.ch>, CERN
/// \author Nima Zardoshti <nima.zardoshti@cern.ch>, CERN

#include "Framework/AnalysisTask.h"
#include "Framework/HistogramRegistry.h"
#include "DetectorsVertexing/DCAFitterN.h"
#include "PWGHF/DataModel/HFSecondaryVertex.h"
#include "Common/Core/trackUtilities.h"
#include "Common/DataModel/EventSelection.h"
//#include "Common/DataModel/Centrality.h"
#include "Common/DataModel/StrangenessTables.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "ReconstructionDataFormats/V0.h"
#include "PWGHF/Utils/UtilsDebugLcK0Sp.h"

#include <algorithm>

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using namespace o2::aod;
using namespace o2::analysis::hf_cuts_single_track;

// enum for candidate type
enum CandidateType {
  Cand2Prong = 0,
  Cand3Prong,
  CandV0bachelor,
  NCandidateTypes
};

// event rejection types
enum EventRejection {
  Trigger = 0,
  PositionX,
  PositionY,
  PositionZ,
  NContrib,
  Chi2,
  NEventRejection
};

static const double massPi = RecoDecay::getMassPDG(kPiPlus);
static const double massK = RecoDecay::getMassPDG(kKPlus);
static const double massProton = RecoDecay::getMassPDG(kProton);
static const double massElectron = RecoDecay::getMassPDG(kElectron);
static const double massMuon = RecoDecay::getMassPDG(kMuonPlus);

void customize(std::vector<o2::framework::ConfigParamSpec>& workflowOptions)
{
  ConfigParamSpec optionDoMC{"doCascades", VariantType::Bool, false, {"Skim also Λc -> K0S p"}};
  ConfigParamSpec optionEvSel{"doTrigSel", VariantType::Bool, false, {"Apply trigger selection"}};
  workflowOptions.push_back(optionDoMC);
  workflowOptions.push_back(optionEvSel);
}

#include "Framework/runDataProcessing.h"

//#define MY_DEBUG

#ifdef MY_DEBUG
using MY_TYPE1 = soa::Join<aod::Tracks, aod::TracksCov, aod::TracksExtra, aod::TracksExtended, aod::McTrackLabels>;
using MyTracks = soa::Join<aod::FullTracks, aod::TracksCov, aod::HFSelTrack, aod::TracksExtended, aod::McTrackLabels>;
#define MY_DEBUG_MSG(condition, cmd) \
  if (condition) {                   \
    cmd;                             \
  }
#else
using MY_TYPE1 = soa::Join<aod::Tracks, aod::TracksCov, aod::TracksExtra, aod::TracksExtended>;
using MyTracks = soa::Join<aod::FullTracks, aod::TracksCov, aod::HFSelTrack, aod::TracksExtended>;
#define MY_DEBUG_MSG(condition, cmd)
#endif

/// Event selection
struct HfTagSelCollisions {
  Produces<aod::HFSelCollision> rowSelectedCollision;

  Configurable<bool> fillHistograms{"fillHistograms", true, "fill histograms"};
  Configurable<double> xVertexMin{"xVertexMin", -100., "min. x of primary vertex [cm]"};
  Configurable<double> xVertexMax{"xVertexMax", 100., "max. x of primary vertex [cm]"};
  Configurable<double> yVertexMin{"yVertexMin", -100., "min. y of primary vertex [cm]"};
  Configurable<double> yVertexMax{"yVertexMax", 100., "max. y of primary vertex [cm]"};
  Configurable<double> zVertexMin{"zVertexMin", -100., "min. z of primary vertex [cm]"};
  Configurable<double> zVertexMax{"zVertexMax", 100., "max. z of primary vertex [cm]"};
  Configurable<int> nContribMin{"nContribMin", 0, "min. number of contributors to primary-vertex reconstruction"};
  Configurable<double> chi2Max{"chi2Max", 0., "max. chi^2 of primary-vertex reconstruction"};
  Configurable<std::string> triggerClassName{"triggerClassName", "kINT7", "trigger class"};
  int triggerClass = std::distance(aliasLabels, std::find(aliasLabels, aliasLabels + kNaliases, triggerClassName.value.data()));

  HistogramRegistry registry{"registry", {{"hNContributors", "Number of vertex contributors;entries", {HistType::kTH1F, {{20001, -0.5, 20000.5}}}}}};

  void init(InitContext const&)
  {
    const int nBinsEvents = 2 + EventRejection::NEventRejection;
    std::string labels[nBinsEvents];
    labels[0] = "processed";
    labels[1] = "selected";
    labels[2 + EventRejection::Trigger] = "rej. trigger";
    labels[2 + EventRejection::PositionX] = "rej. #it{x}";
    labels[2 + EventRejection::PositionY] = "rej. #it{y}";
    labels[2 + EventRejection::PositionZ] = "rej. #it{z}";
    labels[2 + EventRejection::NContrib] = "rej. # of contributors";
    labels[2 + EventRejection::Chi2] = "rej. #it{#chi}^{2}";
    AxisSpec axisEvents = {nBinsEvents, 0.5, nBinsEvents + 0.5, ""};
    registry.add("hEvents", "Events;;entries", HistType::kTH1F, {axisEvents});
    for (int iBin = 0; iBin < nBinsEvents; iBin++) {
      registry.get<TH1>(HIST("hEvents"))->GetXaxis()->SetBinLabel(iBin + 1, labels[iBin].data());
    }
  }

  /// Primary-vertex selection
  /// \param collision  collision table row
  /// \param statusCollision  bit map with rejection results
  template <typename Col>
  void selectVertex(const Col& collision, int& statusCollision)
  {
    if (fillHistograms) {
      registry.fill(HIST("hNContributors"), collision.numContrib());
    }

    // x position
    if (collision.posX() < xVertexMin || collision.posX() > xVertexMax) {
      SETBIT(statusCollision, EventRejection::PositionX);
      if (fillHistograms) {
        registry.fill(HIST("hEvents"), 3 + EventRejection::PositionX);
      }
    }
    // y position
    if (collision.posY() < yVertexMin || collision.posY() > yVertexMax) {
      SETBIT(statusCollision, EventRejection::PositionY);
      if (fillHistograms) {
        registry.fill(HIST("hEvents"), 3 + EventRejection::PositionY);
      }
    }
    // z position
    if (collision.posZ() < zVertexMin || collision.posZ() > zVertexMax) {
      SETBIT(statusCollision, EventRejection::PositionZ);
      if (fillHistograms) {
        registry.fill(HIST("hEvents"), 3 + EventRejection::PositionZ);
      }
    }
    // number of contributors
    if (collision.numContrib() < nContribMin) {
      SETBIT(statusCollision, EventRejection::NContrib);
      if (fillHistograms) {
        registry.fill(HIST("hEvents"), 3 + EventRejection::NContrib);
      }
    }
    // chi^2
    if (chi2Max > 0. && collision.chi2() > chi2Max) {
      SETBIT(statusCollision, EventRejection::Chi2);
      if (fillHistograms) {
        registry.fill(HIST("hEvents"), 3 + EventRejection::Chi2);
      }
    }
  }

  /// Event selection with trigger selection
  void processTrigSel(soa::Join<aod::Collisions, aod::EvSels>::iterator const& collision)
  {
    int statusCollision = 0;

    // processed events
    if (fillHistograms) {
      registry.fill(HIST("hEvents"), 1);
    }

    // trigger selection
    if (!collision.alias()[triggerClass]) {
      SETBIT(statusCollision, EventRejection::Trigger);
      if (fillHistograms) {
        registry.fill(HIST("hEvents"), 3 + EventRejection::Trigger);
      }
    }

    // vertex selection
    selectVertex(collision, statusCollision);

    //TODO: add more event selection criteria

    // selected events
    if (fillHistograms && statusCollision == 0) {
      registry.fill(HIST("hEvents"), 2);
    }

    // fill table row
    rowSelectedCollision(statusCollision);
  };

  PROCESS_SWITCH(HfTagSelCollisions, processTrigSel, "Use trigger selection", true);

  /// Event selection without trigger selection
  void processNoTrigSel(aod::Collision const& collision)
  {
    int statusCollision = 0;

    // processed events
    if (fillHistograms) {
      registry.fill(HIST("hEvents"), 1);
    }

    // vertex selection
    selectVertex(collision, statusCollision);

    // selected events
    if (fillHistograms && statusCollision == 0) {
      registry.fill(HIST("hEvents"), 2);
    }

    // fill table row
    rowSelectedCollision(statusCollision);
  };

  PROCESS_SWITCH(HfTagSelCollisions, processNoTrigSel, "Do not use trigger selection", false);
};

/// Track selection
struct HfTagSelTracks {
  Produces<aod::HFSelTrack> rowSelectedTrack;

  Configurable<bool> fillHistograms{"fillHistograms", true, "fill histograms"};
  Configurable<bool> debug{"debug", true, "debug mode"};
  Configurable<double> bz{"bz", 5., "bz field"};
  // quality cut
  Configurable<bool> doCutQuality{"doCutQuality", true, "apply quality cuts"};
  Configurable<int> tpcNClsFound{"tpcNClsFound", 70, ">= min. number of TPC clusters needed"};
  // pT bins for single-track cuts
  Configurable<std::vector<double>> pTBinsTrack{"pTBinsTrack", std::vector<double>{hf_cuts_single_track::pTBinsTrack_v}, "track pT bin limits for 2-prong DCAXY pT-depentend cut"};
  // 2-prong cuts
  Configurable<double> pTMinTrack2Prong{"pTMinTrack2Prong", -1., "min. track pT for 2 prong candidate"};
  Configurable<LabeledArray<double>> cutsTrack2Prong{"cutsTrack2Prong", {hf_cuts_single_track::cutsTrack[0], npTBinsTrack, nCutVarsTrack, pTBinLabelsTrack, cutVarLabelsTrack}, "Single-track selections per pT bin for 2-prong candidates"};
  Configurable<double> etaMax2Prong{"etaMax2Prong", 4., "max. pseudorapidity for 2 prong candidate"};
  // 3-prong cuts
  Configurable<double> pTMinTrack3Prong{"pTMinTrack3Prong", -1., "min. track pT for 3 prong candidate"};
  Configurable<LabeledArray<double>> cutsTrack3Prong{"cutsTrack3Prong", {hf_cuts_single_track::cutsTrack[0], npTBinsTrack, nCutVarsTrack, pTBinLabelsTrack, cutVarLabelsTrack}, "Single-track selections per pT bin for 3-prong candidates"};
  Configurable<double> etaMax3Prong{"etaMax3Prong", 4., "max. pseudorapidity for 3 prong candidate"};
  // bachelor cuts (when using cascades)
  Configurable<double> ptMinTrackBach{"ptMinTrackBach", 0.3, "min. track pT for bachelor in cascade candidate"}; // 0.5 for PbPb 2015?
  Configurable<LabeledArray<double>> cutsTrackBach{"cutsTrackBach", {hf_cuts_single_track::cutsTrack[0], npTBinsTrack, nCutVarsTrack, pTBinLabelsTrack, cutVarLabelsTrack}, "Single-track selections per pT bin for the bachelor of V0-bachelor candidates"};
  Configurable<double> etaMaxBach{"etaMaxBach", 0.8, "max. pseudorapidity for bachelor in cascade candidate"};

  // for debugging
#ifdef MY_DEBUG
  Configurable<std::vector<int>> indexK0Spos{"indexK0Spos", {729, 2866, 4754, 5457, 6891, 7824, 9243, 9810}, "indices of K0S positive daughters, for debug"};
  Configurable<std::vector<int>> indexK0Sneg{"indexK0Sneg", {730, 2867, 4755, 5458, 6892, 7825, 9244, 9811}, "indices of K0S negative daughters, for debug"};
  Configurable<std::vector<int>> indexProton{"indexProton", {717, 2810, 4393, 5442, 6769, 7793, 9002, 9789}, "indices of protons, for debug"};
#endif

  HistogramRegistry registry{
    "registry",
    {{"hRejTracks", "Tracks;;entries", {HistType::kTH1F, {{15, 0.5, 15.5}}}},
     {"hPtNoCuts", "all tracks;#it{p}_{T}^{track} (GeV/#it{c});entries", {HistType::kTH1F, {{100, 0., 10.}}}},
     // 2-prong histograms
     {"hPtCuts2Prong", "tracks selected for 2-prong vertexing;#it{p}_{T}^{track} (GeV/#it{c});entries", {HistType::kTH1F, {{100, 0., 10.}}}},
     {"hDCAToPrimXYVsPtCuts2Prong", "tracks selected for 2-prong vertexing;#it{p}_{T}^{track} (GeV/#it{c});DCAxy to prim. vtx. (cm);entries", {HistType::kTH2F, {{100, 0., 10.}, {400, -2., 2.}}}},
     {"hEtaCuts2Prong", "tracks selected for 2-prong vertexing;#it{#eta};entries", {HistType::kTH1F, {{static_cast<int>(1.2 * etaMax2Prong * 100), -1.2 * etaMax2Prong, 1.2 * etaMax2Prong}}}},
     // 3-prong histograms
     {"hPtCuts3Prong", "tracks selected for 3-prong vertexing;#it{p}_{T}^{track} (GeV/#it{c});entries", {HistType::kTH1F, {{100, 0., 10.}}}},
     {"hDCAToPrimXYVsPtCuts3Prong", "tracks selected for 3-prong vertexing;#it{p}_{T}^{track} (GeV/#it{c});DCAxy to prim. vtx. (cm);entries", {HistType::kTH2F, {{100, 0., 10.}, {400, -2., 2.}}}},
     {"hEtaCuts3Prong", "tracks selected for 3-prong vertexing;#it{#eta};entries", {HistType::kTH1F, {{static_cast<int>(1.2 * etaMax3Prong * 100), -1.2 * etaMax3Prong, 1.2 * etaMax3Prong}}}},
     // bachelor (for cascades) histograms
     {"hPtCutsV0bachelor", "tracks selected for 3-prong vertexing;#it{p}_{T}^{track} (GeV/#it{c});entries", {HistType::kTH1F, {{100, 0., 10.}}}},
     {"hDCAToPrimXYVsPtCutsV0bachelor", "tracks selected for V0-bachelor vertexing;#it{p}_{T}^{track} (GeV/#it{c});DCAxy to prim. vtx. (cm);entries", {HistType::kTH2F, {{100, 0., 10.}, {400, -2., 2.}}}},
     {"hEtaCutsV0bachelor", "tracks selected for 3-prong vertexing;#it{#eta};entries", {HistType::kTH1F, {{static_cast<int>(1.2 * etaMaxBach * 100), -1.2 * etaMaxBach, 1.2 * etaMaxBach}}}}}};

  static const int nCuts = 4;

  // array of 2-prong and 3-prong single-track cuts
  std::array<LabeledArray<double>, 3> cutsSingleTrack;

  void init(InitContext const&)
  {
    cutsSingleTrack = {cutsTrack2Prong, cutsTrack3Prong, cutsTrackBach};
    std::string cutNames[nCuts + 1] = {"selected", "rej pT", "rej eta", "rej track quality", "rej dca"};
    std::string candNames[CandidateType::NCandidateTypes] = {"2-prong", "3-prong", "bachelor"};
    for (int iCandType = 0; iCandType < CandidateType::NCandidateTypes; iCandType++) {
      for (int iCut = 0; iCut < nCuts + 1; iCut++) {
        registry.get<TH1>(HIST("hRejTracks"))->GetXaxis()->SetBinLabel((nCuts + 1) * iCandType + iCut + 1, Form("%s %s", candNames[iCandType].data(), cutNames[iCut].data()));
      }
    }
  }

  /// Single-track cuts for 2-prongs or 3-prongs
  /// \param hfTrack is a track
  /// \param dca is a 2-element array with dca in transverse and longitudinal directions
  /// \return true if track passes all cuts
  template <typename T>
  bool isSelectedTrack(const T& hfTrack, const array<float, 2>& dca, const int candType)
  {
    auto pTBinTrack = findBin(pTBinsTrack, hfTrack.pt());
    if (pTBinTrack == -1) {
      return false;
    }

    if (std::abs(dca[0]) < cutsSingleTrack[candType].get(pTBinTrack, "min_dcaxytoprimary")) {
      return false; //minimum DCAxy
    }
    if (std::abs(dca[0]) > cutsSingleTrack[candType].get(pTBinTrack, "max_dcaxytoprimary")) {
      return false; //maximum DCAxy
    }
    return true;
  }

  void process(aod::Collisions const& collisions,
               MY_TYPE1 const& tracks
#ifdef MY_DEBUG
               ,
               aod::McParticles& mcParticles
#endif
  )
  {
    for (auto& track : tracks) {

#ifdef MY_DEBUG
      auto indexBach = track.mcParticleId();
      //      LOG(info) << "Checking label " << indexBach;
      bool isProtonFromLc = isProtonFromLcFunc(indexBach, indexProton);

#endif

      MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "\nWe found the proton " << indexBach);

      int statusProng = BIT(CandidateType::NCandidateTypes) - 1; // selection flag , all bits on
      //bool cutStatus[CandidateType::NCandidateTypes][nCuts];
      //if (debug) {
      //  for (int iCandType = 0; iCandType < CandidateType::NCandidateTypes; iCandType++) {
      //    for (int iCut = 0; iCut < nCuts; iCut++) {
      //      cutStatus[iCandType][iCut] = true;
      //    }
      //  }
      //}

      auto trackPt = track.pt();
      auto trackEta = track.eta();

      if (fillHistograms) {
        registry.fill(HIST("hPtNoCuts"), trackPt);
      }

      int iDebugCut = 2;
      // pT cut
      if (trackPt < pTMinTrack2Prong) {
        CLRBIT(statusProng, CandidateType::Cand2Prong); // set the nth bit to 0
        if (debug) {
          //cutStatus[CandidateType::Cand2Prong][0] = false;
          if (fillHistograms) {
            registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::Cand2Prong + iDebugCut);
          }
        }
      }
      if (trackPt < pTMinTrack3Prong) {
        CLRBIT(statusProng, CandidateType::Cand3Prong);
        if (debug) {
          //cutStatus[CandidateType::Cand3Prong][0] = false;
          if (fillHistograms) {
            registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::Cand3Prong + iDebugCut);
          }
        }
      }
      MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "proton " << indexBach << " pt = " << trackPt << " (cut " << ptMinTrackBach << ")");

      if (trackPt < ptMinTrackBach) {
        CLRBIT(statusProng, CandidateType::CandV0bachelor);
        if (debug) {
          //cutStatus[CandidateType::CandV0bachelor][0] = false;
          if (fillHistograms) {
            registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::CandV0bachelor + iDebugCut);
          }
        }
      }

      iDebugCut = 3;
      // eta cut
      if ((debug || TESTBIT(statusProng, CandidateType::Cand2Prong)) && std::abs(trackEta) > etaMax2Prong) {
        CLRBIT(statusProng, CandidateType::Cand2Prong);
        if (debug) {
          //cutStatus[CandidateType::Cand2Prong][1] = false;
          if (fillHistograms) {
            registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::Cand2Prong + iDebugCut);
          }
        }
      }
      if ((debug || TESTBIT(statusProng, CandidateType::Cand3Prong)) && std::abs(trackEta) > etaMax3Prong) {
        CLRBIT(statusProng, CandidateType::Cand3Prong);
        if (debug) {
          //cutStatus[CandidateType::Cand3Prong][1] = false;
          if (fillHistograms) {
            registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::Cand3Prong + iDebugCut);
          }
        }
      }
      MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "proton " << indexBach << " eta = " << trackEta << " (cut " << etaMaxBach << ")");

      if ((debug || TESTBIT(statusProng, CandidateType::CandV0bachelor)) && std::abs(trackEta) > etaMaxBach) {
        CLRBIT(statusProng, CandidateType::CandV0bachelor);
        if (debug) {
          //cutStatus[CandidateType::CandV0bachelor][1] = false;
          if (fillHistograms) {
            registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::CandV0bachelor + iDebugCut);
          }
        }
      }

      // quality cut
      MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "proton " << indexBach << " tpcNClsFound = " << track.tpcNClsFound() << " (cut " << tpcNClsFound.value << ")");

      iDebugCut = 4;
      if (doCutQuality.value && (debug || statusProng > 0)) { // FIXME to make a more complete selection e.g track.flags() & o2::aod::track::TPCrefit && track.flags() & o2::aod::track::GoldenChi2 &&
        UChar_t clustermap = track.itsClusterMap();
        if (!(track.tpcNClsFound() >= tpcNClsFound.value && // is this the number of TPC clusters? It should not be used
              track.flags() & o2::aod::track::ITSrefit &&
              (TESTBIT(clustermap, 0) || TESTBIT(clustermap, 1)))) {
          statusProng = 0;
          MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "proton " << indexBach << " did not pass clusters cut");
          if (debug) {
            for (int iCandType = 0; iCandType < CandidateType::NCandidateTypes; iCandType++) {
              //cutStatus[iCandType][2] = false;
              if (fillHistograms) {
                registry.fill(HIST("hRejTracks"), (nCuts + 1) * iCandType + iDebugCut);
              }
            }
          }
        }
      }

      iDebugCut = 5;
      // DCA cut
      array<float, 2> dca{track.dcaXY(), track.dcaZ()};
      if ((debug || statusProng > 0)) {
        if ((debug || TESTBIT(statusProng, CandidateType::Cand2Prong)) && !isSelectedTrack(track, dca, CandidateType::Cand2Prong)) {
          CLRBIT(statusProng, CandidateType::Cand2Prong);
          if (debug) {
            //cutStatus[CandidateType::Cand2Prong][3] = false;
            if (fillHistograms) {
              registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::Cand2Prong + iDebugCut);
            }
          }
        }
        if ((debug || TESTBIT(statusProng, CandidateType::Cand3Prong)) && !isSelectedTrack(track, dca, CandidateType::Cand3Prong)) {
          CLRBIT(statusProng, CandidateType::Cand3Prong);
          if (debug) {
            //cutStatus[CandidateType::Cand3Prong][3] = false;
            if (fillHistograms) {
              registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::Cand3Prong + iDebugCut);
            }
          }
        }
        if ((debug || TESTBIT(statusProng, CandidateType::CandV0bachelor)) && !isSelectedTrack(track, dca, CandidateType::CandV0bachelor)) {
          CLRBIT(statusProng, CandidateType::CandV0bachelor);
          if (debug) {
            //cutStatus[CandidateType::CandV0bachelor][3] = false;
            if (fillHistograms) {
              registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::CandV0bachelor + iDebugCut);
            }
          }
        }
      }
      MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "statusProng = " << statusProng; printf("\n"));

      // fill histograms
      if (fillHistograms) {
        iDebugCut = 1;
        if (TESTBIT(statusProng, CandidateType::Cand2Prong)) {
          registry.fill(HIST("hPtCuts2Prong"), trackPt);
          registry.fill(HIST("hEtaCuts2Prong"), trackEta);
          registry.fill(HIST("hDCAToPrimXYVsPtCuts2Prong"), trackPt, dca[0]);
          if (debug) {
            registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::Cand2Prong + iDebugCut);
          }
        }
        if (TESTBIT(statusProng, CandidateType::Cand3Prong)) {
          registry.fill(HIST("hPtCuts3Prong"), trackPt);
          registry.fill(HIST("hEtaCuts3Prong"), trackEta);
          registry.fill(HIST("hDCAToPrimXYVsPtCuts3Prong"), trackPt, dca[0]);
          if (debug) {
            registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::Cand3Prong + iDebugCut);
          }
        }
        if (TESTBIT(statusProng, CandidateType::CandV0bachelor)) {
          MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "Will be kept: Proton from Lc " << indexBach);
          registry.fill(HIST("hPtCutsV0bachelor"), trackPt);
          registry.fill(HIST("hEtaCutsV0bachelor"), trackEta);
          registry.fill(HIST("hDCAToPrimXYVsPtCutsV0bachelor"), trackPt, dca[0]);
          if (debug) {
            registry.fill(HIST("hRejTracks"), (nCuts + 1) * CandidateType::CandV0bachelor + iDebugCut);
          }
        }
      }

      // fill table row
      rowSelectedTrack(statusProng, track.px(), track.py(), track.pz());
    }
  }
};

//____________________________________________________________________________________________________________________________________________

/// Pre-selection of 2-prong and 3-prong secondary vertices
struct HfTrackIndexSkimsCreator {
  Produces<aod::HfTrackIndexProng2> rowTrackIndexProng2;
  Produces<aod::HfCutStatusProng2> rowProng2CutStatus;
  Produces<aod::HfTrackIndexProng3> rowTrackIndexProng3;
  Produces<aod::HfCutStatusProng3> rowProng3CutStatus;

  //Configurable<int> nCollsMax{"nCollsMax", -1, "Max collisions per file"}; //can be added to run over limited collisions per file - for tesing purposes
  Configurable<bool> debug{"debug", false, "debug mode"};
  Configurable<bool> fillHistograms{"fillHistograms", true, "fill histograms"};
  Configurable<int> do3prong{"do3prong", 0, "do 3 prong"};
  // preselection parameters
  Configurable<double> pTTolerance{"pTTolerance", 0.1, "pT tolerance in GeV/c for applying preselections before vertex reconstruction"};
  // vertexing parameters
  Configurable<double> bz{"bz", 5., "magnetic field kG"};
  Configurable<bool> propToDCA{"propToDCA", true, "create tracks version propagated to PCA"};
  Configurable<bool> useAbsDCA{"useAbsDCA", true, "Minimise abs. distance rather than chi2"};
  Configurable<double> maxRad{"maxRad", 200., "reject PCA's above this radius"};
  Configurable<double> maxDZIni{"maxDZIni", 4., "reject (if>0) PCA candidate if tracks DZ exceeds threshold"};
  Configurable<double> minParamChange{"minParamChange", 1.e-3, "stop iterations if largest change of any X is smaller than this"};
  Configurable<double> minRelChi2Change{"minRelChi2Change", 0.9, "stop iterations if chi2/chi2old > this"};
  // D0 cuts
  Configurable<std::vector<double>> pTBinsD0ToPiK{"pTBinsD0ToPiK", std::vector<double>{hf_cuts_presel_2prong::pTBinsVec}, "pT bin limits for D0->piK pT-depentend cuts"};
  Configurable<LabeledArray<double>> cutsD0ToPiK{"cutsD0ToPiK", {hf_cuts_presel_2prong::cuts[0], hf_cuts_presel_2prong::npTBins, hf_cuts_presel_2prong::nCutVars, hf_cuts_presel_2prong::pTBinLabels, hf_cuts_presel_2prong::cutVarLabels}, "D0->piK selections per pT bin"};
  // Jpsi -> ee cuts
  Configurable<std::vector<double>> pTBinsJpsiToEE{"pTBinsJpsiToEE", std::vector<double>{hf_cuts_presel_2prong::pTBinsVec}, "pT bin limits for Jpsi->ee pT-depentend cuts"};
  Configurable<LabeledArray<double>> cutsJpsiToEE{"cutsJpsiToEE", {hf_cuts_presel_2prong::cuts[0], hf_cuts_presel_2prong::npTBins, hf_cuts_presel_2prong::nCutVars, hf_cuts_presel_2prong::pTBinLabels, hf_cuts_presel_2prong::cutVarLabels}, "Jpsi->ee selections per pT bin"};
  // Jpsi -> mumu cuts
  Configurable<std::vector<double>> pTBinsJpsiToMuMu{"pTBinsJpsiToMuMu", std::vector<double>{hf_cuts_presel_2prong::pTBinsVec}, "pT bin limits for Jpsi->mumu pT-depentend cuts"};
  Configurable<LabeledArray<double>> cutsJpsiToMuMu{"cutsJpsiToMuMu", {hf_cuts_presel_2prong::cuts[0], hf_cuts_presel_2prong::npTBins, hf_cuts_presel_2prong::nCutVars, hf_cuts_presel_2prong::pTBinLabels, hf_cuts_presel_2prong::cutVarLabels}, "Jpsi->mumu selections per pT bin"};
  // D+ cuts
  Configurable<std::vector<double>> pTBinsDPlusToPiKPi{"pTBinsDPlusToPiKPi", std::vector<double>{hf_cuts_presel_3prong::pTBinsVec}, "pT bin limits for D+->piKpi pT-depentend cuts"};
  Configurable<LabeledArray<double>> cutsDPlusToPiKPi{"cutsDPlusToPiKPi", {hf_cuts_presel_3prong::cuts[0], hf_cuts_presel_3prong::npTBins, hf_cuts_presel_3prong::nCutVars, hf_cuts_presel_3prong::pTBinLabels, hf_cuts_presel_3prong::cutVarLabels}, "D+->piKpi selections per pT bin"};
  // Ds+ cuts
  Configurable<std::vector<double>> pTBinsDsToPiKK{"pTBinsDsToPiKK", std::vector<double>{hf_cuts_presel_3prong::pTBinsVec}, "pT bin limits for Ds+->KKpi pT-depentend cuts"};
  Configurable<LabeledArray<double>> cutsDsToPiKK{"cutsDsToPiKK", {hf_cuts_presel_3prong::cuts[0], hf_cuts_presel_3prong::npTBins, hf_cuts_presel_3prong::nCutVars, hf_cuts_presel_3prong::pTBinLabels, hf_cuts_presel_3prong::cutVarLabels}, "Ds+->KKpi selections per pT bin"};
  // Lc+ cuts
  Configurable<std::vector<double>> pTBinsLcToPKPi{"pTBinsLcToPKPi", std::vector<double>{hf_cuts_presel_3prong::pTBinsVec}, "pT bin limits for Lc->pKpi pT-depentend cuts"};
  Configurable<LabeledArray<double>> cutsLcToPKPi{"cutsLcToPKPi", {hf_cuts_presel_3prong::cuts[0], hf_cuts_presel_3prong::npTBins, hf_cuts_presel_3prong::nCutVars, hf_cuts_presel_3prong::pTBinLabels, hf_cuts_presel_3prong::cutVarLabels}, "Lc->pKpi selections per pT bin"};
  // Xic+ cuts
  Configurable<std::vector<double>> pTBinsXicToPKPi{"pTBinsXicToPKPi", std::vector<double>{hf_cuts_presel_3prong::pTBinsVec}, "pT bin limits for Xic->pKpi pT-depentend cuts"};
  Configurable<LabeledArray<double>> cutsXicToPKPi{"cutsXicToPKPi", {hf_cuts_presel_3prong::cuts[0], hf_cuts_presel_3prong::npTBins, hf_cuts_presel_3prong::nCutVars, hf_cuts_presel_3prong::pTBinLabels, hf_cuts_presel_3prong::cutVarLabels}, "Xic->pKpi selections per pT bin"};

  HistogramRegistry registry{
    "registry",
    {{"hNTracks", ";# of tracks;entries", {HistType::kTH1F, {{2500, 0., 25000.}}}},
     // 2-prong histograms
     {"hVtx2ProngX", "2-prong candidates;#it{x}_{sec. vtx.} (cm);entries", {HistType::kTH1F, {{1000, -2., 2.}}}},
     {"hVtx2ProngY", "2-prong candidates;#it{y}_{sec. vtx.} (cm);entries", {HistType::kTH1F, {{1000, -2., 2.}}}},
     {"hVtx2ProngZ", "2-prong candidates;#it{z}_{sec. vtx.} (cm);entries", {HistType::kTH1F, {{1000, -20., 20.}}}},
     {"hNCand2Prong", "2-prong candidates preselected;# of candidates;entries", {HistType::kTH1F, {{2000, 0., 200000.}}}},
     {"hNCand2ProngVsNTracks", "2-prong candidates preselected;# of selected tracks;# of candidates;entries", {HistType::kTH2F, {{2500, 0., 25000.}, {2000, 0., 200000.}}}},
     {"hmassD0ToPiK", "D^{0} candidates;inv. mass (#pi K) (GeV/#it{c}^{2});entries", {HistType::kTH1F, {{500, 0., 5.}}}},
     {"hmassJpsiToEE", "J/#psi candidates;inv. mass (e^{#plus} e^{#minus}) (GeV/#it{c}^{2});entries", {HistType::kTH1F, {{500, 0., 5.}}}},
     {"hmassJpsiToMuMu", "J/#psi candidates;inv. mass (#mu^{#plus} #mu^{#minus}) (GeV/#it{c}^{2});entries", {HistType::kTH1F, {{500, 0., 5.}}}},
     // 3-prong histograms
     {"hVtx3ProngX", "3-prong candidates;#it{x}_{sec. vtx.} (cm);entries", {HistType::kTH1F, {{1000, -2., 2.}}}},
     {"hVtx3ProngY", "3-prong candidates;#it{y}_{sec. vtx.} (cm);entries", {HistType::kTH1F, {{1000, -2., 2.}}}},
     {"hVtx3ProngZ", "3-prong candidates;#it{z}_{sec. vtx.} (cm);entries", {HistType::kTH1F, {{1000, -20., 20.}}}},
     {"hNCand3Prong", "3-prong candidates preselected;# of candidates;entries", {HistType::kTH1F, {{5000, 0., 500000.}}}},
     {"hNCand3ProngVsNTracks", "3-prong candidates preselected;# of selected tracks;# of candidates;entries", {HistType::kTH2F, {{2500, 0., 25000.}, {5000, 0., 500000.}}}},
     {"hmassDPlusToPiKPi", "D^{#plus} candidates;inv. mass (#pi K #pi) (GeV/#it{c}^{2});entries", {HistType::kTH1F, {{500, 0., 5.}}}},
     {"hmassLcToPKPi", "#Lambda_{c} candidates;inv. mass (p K #pi) (GeV/#it{c}^{2});entries", {HistType::kTH1F, {{500, 0., 5.}}}},
     {"hmassDsToPiKK", "D_{s} candidates;inv. mass (K K #pi) (GeV/#it{c}^{2});entries", {HistType::kTH1F, {{500, 0., 5.}}}},
     {"hmassXicToPKPi", "#Xi_{c} candidates;inv. mass (p K #pi) (GeV/#it{c}^{2});entries", {HistType::kTH1F, {{500, 0., 5.}}}}}};

  static const int n2ProngDecays = hf_cand_prong2::DecayType::N2ProngDecays; // number of 2-prong hadron types
  static const int n3ProngDecays = hf_cand_prong3::DecayType::N3ProngDecays; // number of 3-prong hadron types
  static const int nCuts2Prong = 4;                                          // how many different selections are made on 2-prongs
  static const int nCuts3Prong = 4;                                          // how many different selections are made on 3-prongs

  std::array<std::array<std::array<double, 2>, 2>, n2ProngDecays> arrMass2Prong;
  std::array<std::array<std::array<double, 3>, 2>, n3ProngDecays> arrMass3Prong;

  // arrays of 2-prong and 3-prong cuts
  std::array<LabeledArray<double>, n2ProngDecays> cut2Prong;
  std::array<std::vector<double>, n2ProngDecays> pTBins2Prong;
  std::array<LabeledArray<double>, n3ProngDecays> cut3Prong;
  std::array<std::vector<double>, n3ProngDecays> pTBins3Prong;

  void init(InitContext const&)
  {
    arrMass2Prong[hf_cand_prong2::DecayType::D0ToPiK] = array{array{massPi, massK},
                                                              array{massK, massPi}};

    arrMass2Prong[hf_cand_prong2::DecayType::JpsiToEE] = array{array{massElectron, massElectron},
                                                               array{massElectron, massElectron}};

    arrMass2Prong[hf_cand_prong2::DecayType::JpsiToMuMu] = array{array{massMuon, massMuon},
                                                                 array{massMuon, massMuon}};

    arrMass3Prong[hf_cand_prong3::DecayType::DPlusToPiKPi] = array{array{massPi, massK, massPi},
                                                                   array{massPi, massK, massPi}};

    arrMass3Prong[hf_cand_prong3::DecayType::LcToPKPi] = array{array{massProton, massK, massPi},
                                                               array{massPi, massK, massProton}};

    arrMass3Prong[hf_cand_prong3::DecayType::DsToPiKK] = array{array{massK, massK, massPi},
                                                               array{massPi, massK, massK}};

    arrMass3Prong[hf_cand_prong3::DecayType::XicToPKPi] = array{array{massProton, massK, massPi},
                                                                array{massPi, massK, massProton}};

    // cuts for 2-prong decays retrieved by json. the order must be then one in hf_cand_prong2::DecayType
    cut2Prong = {cutsD0ToPiK, cutsJpsiToEE, cutsJpsiToMuMu};
    pTBins2Prong = {pTBinsD0ToPiK, pTBinsJpsiToEE, pTBinsJpsiToMuMu};
    // cuts for 3-prong decays retrieved by json. the order must be then one in hf_cand_prong3::DecayType
    cut3Prong = {cutsDPlusToPiKPi, cutsLcToPKPi, cutsDsToPiKK, cutsXicToPKPi};
    pTBins3Prong = {pTBinsDPlusToPiKPi, pTBinsLcToPKPi, pTBinsDsToPiKK, pTBinsXicToPKPi};
  }

  /// Method to perform selections for 2-prong candidates before vertex reconstruction
  /// \param hfTrack0 is the first daughter track
  /// \param hfTrack1 is the second daughter track
  /// \param cutStatus is a 2D array with outcome of each selection (filled only in debug mode)
  /// \param whichHypo information of the mass hypoteses that were selected
  /// \param isSelected ia s bitmap with selection outcome
  template <typename T1, typename T2, typename T3>
  void is2ProngPreselected(T1 const& hfTrack0, T1 const& hfTrack1, T2& cutStatus, T3& whichHypo, int& isSelected)
  {
    /// FIXME: this would be better fixed by having a convention on the position of min and max in the 2D Array
    static std::vector<int> massMinIndex;
    static std::vector<int> massMaxIndex;
    static std::vector<int> d0d0Index;
    static auto cacheIndices = [](std::array<LabeledArray<double>, n2ProngDecays>& cut2Prong, std::vector<int>& mins, std::vector<int>& maxs, std::vector<int>& d0d0) {
      mins.resize(cut2Prong.size());
      maxs.resize(cut2Prong.size());
      d0d0.resize(cut2Prong.size());
      for (size_t i = 0; i < cut2Prong.size(); ++i) {
        mins[i] = cut2Prong[i].colmap.find("massMin")->second;
        maxs[i] = cut2Prong[i].colmap.find("massMax")->second;
        d0d0[i] = cut2Prong[i].colmap.find("d0d0")->second;
      }
      return true;
    };
    cacheIndices(cut2Prong, massMinIndex, massMaxIndex, d0d0Index);

    auto arrMom = array{
      array{hfTrack0.pxProng(), hfTrack0.pyProng(), hfTrack0.pzProng()},
      array{hfTrack1.pxProng(), hfTrack1.pyProng(), hfTrack1.pzProng()}};

    auto pT = RecoDecay::Pt(arrMom[0], arrMom[1]) + pTTolerance; // add tolerance because of no reco decay vertex

    for (int iDecay2P = 0; iDecay2P < n2ProngDecays; iDecay2P++) {

      // pT
      auto pTBin = findBin(&pTBins2Prong[iDecay2P], pT);
      // return immediately if it is outside the defined pT bins
      if (pTBin == -1) {
        CLRBIT(isSelected, iDecay2P);
        if (debug) {
          cutStatus[iDecay2P][0] = false;
        }
        continue;
      }

      // invariant mass
      double massHypos[2];
      whichHypo[iDecay2P] = 3;
      double min2 = pow(cut2Prong[iDecay2P].get(pTBin, massMinIndex[iDecay2P]), 2);
      double max2 = pow(cut2Prong[iDecay2P].get(pTBin, massMaxIndex[iDecay2P]), 2);

      if ((debug || TESTBIT(isSelected, iDecay2P)) && cut2Prong[iDecay2P].get(pTBin, massMinIndex[iDecay2P]) >= 0. && cut2Prong[iDecay2P].get(pTBin, massMaxIndex[iDecay2P]) > 0.) {
        massHypos[0] = RecoDecay::M2(arrMom, arrMass2Prong[iDecay2P][0]);
        massHypos[1] = RecoDecay::M2(arrMom, arrMass2Prong[iDecay2P][1]);
        if (massHypos[0] < min2 || massHypos[0] >= max2) {
          whichHypo[iDecay2P] -= 1;
        }
        if (massHypos[1] < min2 || massHypos[1] >= max2) {
          whichHypo[iDecay2P] -= 2;
        }
        if (whichHypo[iDecay2P] == 0) {
          CLRBIT(isSelected, iDecay2P);
          if (debug) {
            cutStatus[iDecay2P][1] = false;
          }
        }
      }

      // imp. par. product cut
      if (debug || TESTBIT(isSelected, iDecay2P)) {
        auto impParProduct = hfTrack0.dcaXY() * hfTrack1.dcaXY();
        if (impParProduct > cut2Prong[iDecay2P].get(pTBin, d0d0Index[iDecay2P])) {
          CLRBIT(isSelected, iDecay2P);
          if (debug) {
            cutStatus[iDecay2P][2] = false;
          }
        }
      }
    }
  }

  /// Method to perform selections for 3-prong candidates before vertex reconstruction
  /// \param hfTrack0 is the first daughter track
  /// \param hfTrack1 is the second daughter track
  /// \param hfTrack2 is the third daughter track
  /// \param cutStatus is a 2D array with outcome of each selection (filled only in debug mode)
  /// \param whichHypo information of the mass hypoteses that were selected
  /// \param isSelected ia s bitmap with selection outcome
  template <typename T1, typename T2, typename T3>
  void is3ProngPreselected(T1 const& hfTrack0, T1 const& hfTrack1, T1 const& hfTrack2, T2& cutStatus, T3& whichHypo, int& isSelected)
  {
    /// FIXME: this would be better fixed by having a convention on the position of min and max in the 2D Array
    static std::vector<int> massMinIndex;
    static std::vector<int> massMaxIndex;
    static auto cacheIndices = [](std::array<LabeledArray<double>, n3ProngDecays>& cut3Prong, std::vector<int>& mins, std::vector<int>& maxs) {
      mins.resize(cut3Prong.size());
      maxs.resize(cut3Prong.size());
      for (size_t iDecay3P = 0; iDecay3P < cut3Prong.size(); ++iDecay3P) {
        mins[iDecay3P] = cut3Prong[iDecay3P].colmap.find("massMin")->second;
        maxs[iDecay3P] = cut3Prong[iDecay3P].colmap.find("massMax")->second;
      }
      return true;
    };
    cacheIndices(cut3Prong, massMinIndex, massMaxIndex);

    auto arrMom = array{
      array{hfTrack0.pxProng(), hfTrack0.pyProng(), hfTrack0.pzProng()},
      array{hfTrack1.pxProng(), hfTrack1.pyProng(), hfTrack1.pzProng()},
      array{hfTrack2.pxProng(), hfTrack2.pyProng(), hfTrack2.pzProng()}};

    auto pT = RecoDecay::Pt(arrMom[0], arrMom[1], arrMom[2]) + pTTolerance; // add tolerance because of no reco decay vertex

    for (int iDecay3P = 0; iDecay3P < n3ProngDecays; iDecay3P++) {

      // pT
      auto pTBin = findBin(&pTBins3Prong[iDecay3P], pT);
      // return immediately if it is outside the defined pT bins
      if (pTBin == -1) {
        CLRBIT(isSelected, iDecay3P);
        if (debug) {
          cutStatus[iDecay3P][0] = false;
        }
        continue;
      }

      // invariant mass
      double massHypos[2];
      whichHypo[iDecay3P] = 3;
      double min2 = pow(cut3Prong[iDecay3P].get(pTBin, massMinIndex[iDecay3P]), 2);
      double max2 = pow(cut3Prong[iDecay3P].get(pTBin, massMaxIndex[iDecay3P]), 2);

      if ((debug || TESTBIT(isSelected, iDecay3P)) && cut3Prong[iDecay3P].get(pTBin, massMinIndex[iDecay3P]) >= 0. && cut3Prong[iDecay3P].get(pTBin, massMaxIndex[iDecay3P]) > 0.) { //no need to check isSelected but to avoid mistakes
        massHypos[0] = RecoDecay::M2(arrMom, arrMass3Prong[iDecay3P][0]);
        massHypos[1] = RecoDecay::M2(arrMom, arrMass3Prong[iDecay3P][1]);
        if (massHypos[0] < min2 || massHypos[0] >= max2) {
          whichHypo[iDecay3P] -= 1;
        }
        if (massHypos[1] < min2 || massHypos[1] >= max2) {
          whichHypo[iDecay3P] -= 2;
        }
        if (whichHypo[iDecay3P] == 0) {
          CLRBIT(isSelected, iDecay3P);
          if (debug) {
            cutStatus[iDecay3P][1] = false;
          }
        }
      }
    }
  }

  /// Method to perform selections for 2-prong candidates after vertex reconstruction
  /// \param pVecCand is the array for the candidate momentum after reconstruction of secondary vertex
  /// \param secVtx is the secondary vertex
  /// \param primVtx is the primary vertex
  /// \param cutStatus is a 2D array with outcome of each selection (filled only in debug mode)
  /// \param isSelected ia s bitmap with selection outcome
  template <typename T1, typename T2, typename T3, typename T4>
  void is2ProngSelected(const T1& pVecCand, const T2& secVtx, const T3& primVtx, T4& cutStatus, int& isSelected)
  {
    if (debug || isSelected > 0) {

      /// FIXME: this would be better fixed by having a convention on the position of min and max in the 2D Array
      static std::vector<int> cospIndex;
      static auto cacheIndices = [](std::array<LabeledArray<double>, n2ProngDecays>& cut2Prong, std::vector<int>& cosp) {
        cosp.resize(cut2Prong.size());
        for (size_t iDecay2P = 0; iDecay2P < cut2Prong.size(); ++iDecay2P) {
          cosp[iDecay2P] = cut2Prong[iDecay2P].colmap.find("cosp")->second;
        }
        return true;
      };
      cacheIndices(cut2Prong, cospIndex);

      for (int iDecay2P = 0; iDecay2P < n2ProngDecays; iDecay2P++) {

        // pT
        auto pTBin = findBin(&pTBins2Prong[iDecay2P], RecoDecay::Pt(pVecCand));
        if (pTBin == -1) { // cut if it is outside the defined pT bins
          CLRBIT(isSelected, iDecay2P);
          if (debug) {
            cutStatus[iDecay2P][0] = false;
          }
          continue;
        }

        // cosp
        if (debug || TESTBIT(isSelected, iDecay2P)) {
          auto cpa = RecoDecay::CPA(primVtx, secVtx, pVecCand);
          if (cpa < cut2Prong[iDecay2P].get(pTBin, cospIndex[iDecay2P])) {
            CLRBIT(isSelected, iDecay2P);
            if (debug) {
              cutStatus[iDecay2P][3] = false;
            }
          }
        }
      }
    }
  }

  /// Method to perform selections for 3-prong candidates after vertex reconstruction
  /// \param pVecCand is the array for the candidate momentum after reconstruction of secondary vertex
  /// \param secVtx is the secondary vertex
  /// \param primVtx is the primary vertex
  /// \param cutStatus is a 2D array with outcome of each selection (filled only in debug mode)
  /// \param isSelected ia s bitmap with selection outcome
  template <typename T1, typename T2, typename T3, typename T4>
  void is3ProngSelected(const T1& pVecCand, const T2& secVtx, const T3& primVtx, T4& cutStatus, int& isSelected)
  {
    if (debug || isSelected > 0) {

      /// FIXME: this would be better fixed by having a convention on the position of min and max in the 2D Array
      static std::vector<int> cospIndex;
      static std::vector<int> decLenIndex;
      static auto cacheIndices = [](std::array<LabeledArray<double>, n3ProngDecays>& cut3Prong, std::vector<int>& cosp, std::vector<int>& decL) {
        cosp.resize(cut3Prong.size());
        decL.resize(cut3Prong.size());
        for (size_t iDecay3P = 0; iDecay3P < cut3Prong.size(); ++iDecay3P) {
          cosp[iDecay3P] = cut3Prong[iDecay3P].colmap.find("cosp")->second;
          decL[iDecay3P] = cut3Prong[iDecay3P].colmap.find("decL")->second;
        }
        return true;
      };
      cacheIndices(cut3Prong, cospIndex, decLenIndex);

      for (int iDecay3P = 0; iDecay3P < n3ProngDecays; iDecay3P++) {

        // pT
        auto pTBin = findBin(&pTBins3Prong[iDecay3P], RecoDecay::Pt(pVecCand));
        if (pTBin == -1) { // cut if it is outside the defined pT bins
          CLRBIT(isSelected, iDecay3P);
          if (debug) {
            cutStatus[iDecay3P][0] = false;
          }
          continue;
        }

        // cosp
        if ((debug || TESTBIT(isSelected, iDecay3P))) {
          auto cpa = RecoDecay::CPA(primVtx, secVtx, pVecCand);
          if (cpa < cut3Prong[iDecay3P].get(pTBin, cospIndex[iDecay3P])) {
            CLRBIT(isSelected, iDecay3P);
            if (debug) {
              cutStatus[iDecay3P][2] = false;
            }
          }
        }

        // decay length
        if ((debug || TESTBIT(isSelected, iDecay3P))) {
          auto decayLength = RecoDecay::distance(primVtx, secVtx);
          if (decayLength < cut3Prong[iDecay3P].get(pTBin, decLenIndex[iDecay3P])) {
            CLRBIT(isSelected, iDecay3P);
            if (debug) {
              cutStatus[iDecay3P][3] = false;
            }
          }
        }
      }
    }
  }

  Filter filterSelectCollisions = (aod::hf_selcollision::whyRejectColl == 0);
  Filter filterSelectTracks = aod::hf_seltrack::isSelProng > 0;

  using SelectedCollisions = soa::Filtered<soa::Join<aod::Collisions, aod::HFSelCollision>>;
  using SelectedTracks = soa::Filtered<soa::Join<aod::Tracks, aod::TracksCov, aod::TracksExtra, aod::TracksExtended, aod::HFSelTrack>>;

  // FIXME
  //Partition<SelectedTracks> tracksPos = aod::track::signed1Pt > 0.f;
  //Partition<SelectedTracks> tracksNeg = aod::track::signed1Pt < 0.f;

  // int nColls{0}; //can be added to run over limited collisions per file - for tesing purposes

  void process( //soa::Join<aod::Collisions, aod::CentV0Ms>::iterator const& collision, //FIXME add centrality when option for variations to the process function appears
    SelectedCollisions::iterator const& collision,
    aod::BCs const& bcs,
    SelectedTracks const& tracks)
  {

    //can be added to run over limited collisions per file - for tesing purposes
    /*
    if (nCollsMax > -1){
      if (nColls == nCollMax){
        return;
        //can be added to run over limited collisions per file - for tesing purposes
      }
      nColls++;
    }
    */

    //auto centrality = collision.centV0M(); //FIXME add centrality when option for variations to the process function appears

    int n2ProngBit = BIT(n2ProngDecays) - 1; // bit value for 2-prong candidates where each candidiate is one bit and they are all set to 1
    int n3ProngBit = BIT(n3ProngDecays) - 1; // bit value for 3-prong candidates where each candidiate is one bit and they are all set to 1

    bool cutStatus2Prong[n2ProngDecays][nCuts2Prong];
    bool cutStatus3Prong[n3ProngDecays][nCuts3Prong];
    int nCutStatus2ProngBit = BIT(nCuts2Prong) - 1; // bit value for selection status for each 2-prong candidate where each selection is one bit and they are all set to 1
    int nCutStatus3ProngBit = BIT(nCuts3Prong) - 1; // bit value for selection status for each 3-prong candidate where each selection is one bit and they are all set to 1

    int whichHypo2Prong[n2ProngDecays];
    int whichHypo3Prong[n3ProngDecays];

    // 2-prong vertex fitter
    o2::vertexing::DCAFitterN<2> df2;
    df2.setBz(bz);
    df2.setPropagateToPCA(propToDCA);
    df2.setMaxR(maxRad);
    df2.setMaxDZIni(maxDZIni);
    df2.setMinParamChange(minParamChange);
    df2.setMinRelChi2Change(minRelChi2Change);
    df2.setUseAbsDCA(useAbsDCA);

    // 3-prong vertex fitter
    o2::vertexing::DCAFitterN<3> df3;
    df3.setBz(bz);
    df3.setPropagateToPCA(propToDCA);
    df3.setMaxR(maxRad);
    df3.setMaxDZIni(maxDZIni);
    df3.setMinParamChange(minParamChange);
    df3.setMinRelChi2Change(minRelChi2Change);
    df3.setUseAbsDCA(useAbsDCA);

    // used to calculate number of candidiates per event
    auto nCand2 = rowTrackIndexProng2.lastIndex();
    auto nCand3 = rowTrackIndexProng3.lastIndex();

    // if there isn't at least a positive and a negative track, continue immediately
    //if (tracksPos.size() < 1 || tracksNeg.size() < 1) {
    //  return;
    //}

    // first loop over positive tracks
    //for (auto trackPos1 = tracksPos.begin(); trackPos1 != tracksPos.end(); ++trackPos1) {
    for (auto trackPos1 = tracks.begin(); trackPos1 != tracks.end(); ++trackPos1) {
      if (trackPos1.signed1Pt() < 0) {
        continue;
      }
      bool sel2ProngStatusPos = TESTBIT(trackPos1.isSelProng(), CandidateType::Cand2Prong);
      bool sel3ProngStatusPos1 = TESTBIT(trackPos1.isSelProng(), CandidateType::Cand3Prong);
      if (!sel2ProngStatusPos && !sel3ProngStatusPos1) {
        continue;
      }

      auto trackParVarPos1 = getTrackParCov(trackPos1);

      // first loop over negative tracks
      //for (auto trackNeg1 = tracksNeg.begin(); trackNeg1 != tracksNeg.end(); ++trackNeg1) {
      for (auto trackNeg1 = tracks.begin(); trackNeg1 != tracks.end(); ++trackNeg1) {
        if (trackNeg1.signed1Pt() > 0) {
          continue;
        }
        bool sel2ProngStatusNeg = TESTBIT(trackNeg1.isSelProng(), CandidateType::Cand2Prong);
        bool sel3ProngStatusNeg1 = TESTBIT(trackNeg1.isSelProng(), CandidateType::Cand3Prong);
        if (!sel2ProngStatusNeg && !sel3ProngStatusNeg1) {
          continue;
        }

        auto trackParVarNeg1 = getTrackParCov(trackNeg1);

        int isSelected2ProngCand = n2ProngBit; //bitmap for checking status of two-prong candidates (1 is true, 0 is rejected)

        if (debug) {
          for (int iDecay2P = 0; iDecay2P < n2ProngDecays; iDecay2P++) {
            for (int iCut = 0; iCut < nCuts2Prong; iCut++) {
              cutStatus2Prong[iDecay2P][iCut] = true;
            }
          }
        }

        // 2-prong vertex reconstruction
        if (sel2ProngStatusPos && sel2ProngStatusNeg) {

          // 2-prong preselections
          is2ProngPreselected(trackPos1, trackNeg1, cutStatus2Prong, whichHypo2Prong, isSelected2ProngCand);

          // secondary vertex reconstruction and further 2-prong selections
          if (isSelected2ProngCand > 0 && df2.process(trackParVarPos1, trackParVarNeg1) > 0) { //should it be this or > 0 or are they equivalent
            // get secondary vertex
            const auto& secondaryVertex2 = df2.getPCACandidate();
            // get track momenta
            array<float, 3> pvec0;
            array<float, 3> pvec1;
            df2.getTrack(0).getPxPyPzGlo(pvec0);
            df2.getTrack(1).getPxPyPzGlo(pvec1);

            auto pVecCandProng2 = RecoDecay::PVec(pvec0, pvec1);
            // 2-prong selections after secondary vertex
            is2ProngSelected(pVecCandProng2, secondaryVertex2, array{collision.posX(), collision.posY(), collision.posZ()}, cutStatus2Prong, isSelected2ProngCand);

            if (isSelected2ProngCand > 0) {
              // fill table row
              rowTrackIndexProng2(trackPos1.globalIndex(),
                                  trackNeg1.globalIndex(), isSelected2ProngCand);
              if (debug) {
                int Prong2CutStatus[n2ProngDecays];
                for (int iDecay2P = 0; iDecay2P < n2ProngDecays; iDecay2P++) {
                  Prong2CutStatus[iDecay2P] = nCutStatus2ProngBit;
                  for (int iCut = 0; iCut < nCuts2Prong; iCut++) {
                    if (!cutStatus2Prong[iDecay2P][iCut]) {
                      CLRBIT(Prong2CutStatus[iDecay2P], iCut);
                    }
                  }
                }
                rowProng2CutStatus(Prong2CutStatus[0], Prong2CutStatus[1], Prong2CutStatus[2]); //FIXME when we can do this by looping over n2ProngDecays
              }

              // fill histograms
              if (fillHistograms) {
                registry.fill(HIST("hVtx2ProngX"), secondaryVertex2[0]);
                registry.fill(HIST("hVtx2ProngY"), secondaryVertex2[1]);
                registry.fill(HIST("hVtx2ProngZ"), secondaryVertex2[2]);
                array<array<float, 3>, 2> arrMom = {pvec0, pvec1};
                for (int iDecay2P = 0; iDecay2P < n2ProngDecays; iDecay2P++) {
                  if (TESTBIT(isSelected2ProngCand, iDecay2P)) {
                    if (whichHypo2Prong[iDecay2P] == 1 || whichHypo2Prong[iDecay2P] == 3) {
                      auto mass2Prong = RecoDecay::M(arrMom, arrMass2Prong[iDecay2P][0]);
                      switch (iDecay2P) {
                        case hf_cand_prong2::DecayType::D0ToPiK:
                          registry.fill(HIST("hmassD0ToPiK"), mass2Prong);
                          break;
                        case hf_cand_prong2::DecayType::JpsiToEE:
                          registry.fill(HIST("hmassJpsiToEE"), mass2Prong);
                          break;
                        case hf_cand_prong2::DecayType::JpsiToMuMu:
                          registry.fill(HIST("hmassJpsiToMuMu"), mass2Prong);
                          break;
                      }
                    }
                    if (whichHypo2Prong[iDecay2P] >= 2) {
                      auto mass2Prong = RecoDecay::M(arrMom, arrMass2Prong[iDecay2P][1]);
                      if (iDecay2P == hf_cand_prong2::DecayType::D0ToPiK) {
                        registry.fill(HIST("hmassD0ToPiK"), mass2Prong);
                      }
                    }
                  }
                }
              }
            }
          }
        }

        // 3-prong vertex reconstruction
        if (do3prong == 1) {
          if (!sel3ProngStatusPos1 || !sel3ProngStatusNeg1) {
            continue;
          }

          if (tracks.size() < 2) {
            continue;
          }
          // second loop over positive tracks
          //for (auto trackPos2 = trackPos1 + 1; trackPos2 != tracksPos.end(); ++trackPos2) {
          for (auto trackPos2 = trackPos1 + 1; trackPos2 != tracks.end(); ++trackPos2) {
            if (trackPos2.signed1Pt() < 0) {
              continue;
            }
            if (!TESTBIT(trackPos2.isSelProng(), CandidateType::Cand3Prong)) {
              continue;
            }

            int isSelected3ProngCand = n3ProngBit;

            if (debug) {
              for (int iDecay3P = 0; iDecay3P < n3ProngDecays; iDecay3P++) {
                for (int iCut = 0; iCut < nCuts3Prong; iCut++) {
                  cutStatus3Prong[iDecay3P][iCut] = true;
                }
              }
            }

            // 3-prong preselections
            is3ProngPreselected(trackPos1, trackNeg1, trackPos2, cutStatus3Prong, whichHypo3Prong, isSelected3ProngCand);
            if (!debug && isSelected3ProngCand == 0) {
              continue;
            }

            // reconstruct the 3-prong secondary vertex
            auto trackParVarPos2 = getTrackParCov(trackPos2);
            if (df3.process(trackParVarPos1, trackParVarNeg1, trackParVarPos2) == 0) {
              continue;
            }
            // get secondary vertex
            const auto& secondaryVertex3 = df3.getPCACandidate();
            // get track momenta
            array<float, 3> pvec0;
            array<float, 3> pvec1;
            array<float, 3> pvec2;
            df3.getTrack(0).getPxPyPzGlo(pvec0);
            df3.getTrack(1).getPxPyPzGlo(pvec1);
            df3.getTrack(2).getPxPyPzGlo(pvec2);

            auto pVecCandProng3Pos = RecoDecay::PVec(pvec0, pvec1, pvec2);
            // 3-prong selections after secondary vertex
            is3ProngSelected(pVecCandProng3Pos, secondaryVertex3, array{collision.posX(), collision.posY(), collision.posZ()}, cutStatus3Prong, isSelected3ProngCand);
            if (!debug && isSelected3ProngCand == 0) {
              continue;
            }

            // fill table row
            rowTrackIndexProng3(trackPos1.globalIndex(),
                                trackNeg1.globalIndex(),
                                trackPos2.globalIndex(), isSelected3ProngCand);

            if (debug) {
              int Prong3CutStatus[n3ProngDecays];
              for (int iDecay3P = 0; iDecay3P < n3ProngDecays; iDecay3P++) {
                Prong3CutStatus[iDecay3P] = nCutStatus3ProngBit;
                for (int iCut = 0; iCut < nCuts3Prong; iCut++) {
                  if (!cutStatus3Prong[iDecay3P][iCut]) {
                    CLRBIT(Prong3CutStatus[iDecay3P], iCut);
                  }
                }
              }
              rowProng3CutStatus(Prong3CutStatus[0], Prong3CutStatus[1], Prong3CutStatus[2], Prong3CutStatus[3]); //FIXME when we can do this by looping over n3ProngDecays
            }

            // fill histograms
            if (fillHistograms) {
              registry.fill(HIST("hVtx3ProngX"), secondaryVertex3[0]);
              registry.fill(HIST("hVtx3ProngY"), secondaryVertex3[1]);
              registry.fill(HIST("hVtx3ProngZ"), secondaryVertex3[2]);
              array<array<float, 3>, 3> arr3Mom = {pvec0, pvec1, pvec2};
              for (int iDecay3P = 0; iDecay3P < n3ProngDecays; iDecay3P++) {
                if (TESTBIT(isSelected3ProngCand, iDecay3P)) {
                  if (whichHypo3Prong[iDecay3P] == 1 || whichHypo3Prong[iDecay3P] == 3) {
                    auto mass3Prong = RecoDecay::M(arr3Mom, arrMass3Prong[iDecay3P][0]);
                    switch (iDecay3P) {
                      case hf_cand_prong3::DecayType::DPlusToPiKPi:
                        registry.fill(HIST("hmassDPlusToPiKPi"), mass3Prong);
                        break;
                      case hf_cand_prong3::DecayType::DsToPiKK:
                        registry.fill(HIST("hmassDsToPiKK"), mass3Prong);
                        break;
                      case hf_cand_prong3::DecayType::LcToPKPi:
                        registry.fill(HIST("hmassLcToPKPi"), mass3Prong);
                        break;
                      case hf_cand_prong3::DecayType::XicToPKPi:
                        registry.fill(HIST("hmassXicToPKPi"), mass3Prong);
                        break;
                    }
                  }
                  if (whichHypo3Prong[iDecay3P] >= 2) {
                    auto mass3Prong = RecoDecay::M(arr3Mom, arrMass3Prong[iDecay3P][1]);
                    switch (iDecay3P) {
                      case hf_cand_prong3::DecayType::DsToPiKK:
                        registry.fill(HIST("hmassDsToPiKK"), mass3Prong);
                        break;
                      case hf_cand_prong3::DecayType::LcToPKPi:
                        registry.fill(HIST("hmassLcToPKPi"), mass3Prong);
                        break;
                      case hf_cand_prong3::DecayType::XicToPKPi:
                        registry.fill(HIST("hmassXicToPKPi"), mass3Prong);
                        break;
                    }
                  }
                }
              }
            }
          }

          // second loop over negative tracks
          //for (auto trackNeg2 = trackNeg1 + 1; trackNeg2 != tracksNeg.end(); ++trackNeg2) {
          for (auto trackNeg2 = trackNeg1 + 1; trackNeg2 != tracks.end(); ++trackNeg2) {
            if (trackNeg2.signed1Pt() > 0) {
              continue;
            }
            if (!TESTBIT(trackNeg2.isSelProng(), CandidateType::Cand3Prong)) {
              continue;
            }

            int isSelected3ProngCand = n3ProngBit;

            if (debug) {
              for (int iDecay3P = 0; iDecay3P < n3ProngDecays; iDecay3P++) {
                for (int iCut = 0; iCut < nCuts3Prong; iCut++) {
                  cutStatus3Prong[iDecay3P][iCut] = true;
                }
              }
            }

            //3-prong preselections
            is3ProngPreselected(trackNeg1, trackPos1, trackNeg2, cutStatus3Prong, whichHypo3Prong, isSelected3ProngCand);
            if (!debug && isSelected3ProngCand == 0) {
              continue;
            }

            // reconstruct the 3-prong secondary vertex
            auto trackParVarNeg2 = getTrackParCov(trackNeg2);
            if (df3.process(trackParVarNeg1, trackParVarPos1, trackParVarNeg2) == 0) {
              continue;
            }

            // get secondary vertex
            const auto& secondaryVertex3 = df3.getPCACandidate();
            // get track momenta
            array<float, 3> pvec0;
            array<float, 3> pvec1;
            array<float, 3> pvec2;
            df3.getTrack(0).getPxPyPzGlo(pvec0);
            df3.getTrack(1).getPxPyPzGlo(pvec1);
            df3.getTrack(2).getPxPyPzGlo(pvec2);

            auto pVecCandProng3Neg = RecoDecay::PVec(pvec0, pvec1, pvec2);

            // 3-prong selections after secondary vertex
            is3ProngSelected(pVecCandProng3Neg, secondaryVertex3, array{collision.posX(), collision.posY(), collision.posZ()}, cutStatus3Prong, isSelected3ProngCand);
            if (!debug && isSelected3ProngCand == 0) {
              continue;
            }

            // fill table row
            rowTrackIndexProng3(trackNeg1.globalIndex(),
                                trackPos1.globalIndex(),
                                trackNeg2.globalIndex(), isSelected3ProngCand);

            if (debug) {
              int Prong3CutStatus[n3ProngDecays];
              for (int iDecay3P = 0; iDecay3P < n3ProngDecays; iDecay3P++) {
                Prong3CutStatus[iDecay3P] = nCutStatus3ProngBit;
                for (int iCut = 0; iCut < nCuts3Prong; iCut++) {
                  if (!cutStatus3Prong[iDecay3P][iCut]) {
                    CLRBIT(Prong3CutStatus[iDecay3P], iCut);
                  }
                }
              }
              rowProng3CutStatus(Prong3CutStatus[0], Prong3CutStatus[1], Prong3CutStatus[2], Prong3CutStatus[3]); //FIXME when we can do this by looping over n3ProngDecays
            }

            // fill histograms
            if (fillHistograms) {
              registry.fill(HIST("hVtx3ProngX"), secondaryVertex3[0]);
              registry.fill(HIST("hVtx3ProngY"), secondaryVertex3[1]);
              registry.fill(HIST("hVtx3ProngZ"), secondaryVertex3[2]);
              array<array<float, 3>, 3> arr3Mom = {pvec0, pvec1, pvec2};
              for (int iDecay3P = 0; iDecay3P < n3ProngDecays; iDecay3P++) {
                if (TESTBIT(isSelected3ProngCand, iDecay3P)) {
                  if (whichHypo3Prong[iDecay3P] == 1 || whichHypo3Prong[iDecay3P] == 3) {
                    auto mass3Prong = RecoDecay::M(arr3Mom, arrMass3Prong[iDecay3P][0]);
                    switch (iDecay3P) {
                      case hf_cand_prong3::DecayType::DPlusToPiKPi:
                        registry.fill(HIST("hmassDPlusToPiKPi"), mass3Prong);
                        break;
                      case hf_cand_prong3::DecayType::DsToPiKK:
                        registry.fill(HIST("hmassDsToPiKK"), mass3Prong);
                        break;
                      case hf_cand_prong3::DecayType::LcToPKPi:
                        registry.fill(HIST("hmassLcToPKPi"), mass3Prong);
                        break;
                      case hf_cand_prong3::DecayType::XicToPKPi:
                        registry.fill(HIST("hmassXicToPKPi"), mass3Prong);
                        break;
                    }
                  }
                  if (whichHypo3Prong[iDecay3P] >= 2) {
                    auto mass3Prong = RecoDecay::M(arr3Mom, arrMass3Prong[iDecay3P][1]);
                    switch (iDecay3P) {
                      case hf_cand_prong3::DecayType::DsToPiKK:
                        registry.fill(HIST("hmassDsToPiKK"), mass3Prong);
                        break;
                      case hf_cand_prong3::DecayType::LcToPKPi:
                        registry.fill(HIST("hmassLcToPKPi"), mass3Prong);
                        break;
                      case hf_cand_prong3::DecayType::XicToPKPi:
                        registry.fill(HIST("hmassXicToPKPi"), mass3Prong);
                        break;
                    }
                  }
                }
              }
            }
          }
        }
      }
    }

    auto nTracks = tracks.size();                      // number of tracks passing 2 and 3 prong selection in this collision
    nCand2 = rowTrackIndexProng2.lastIndex() - nCand2; // number of 2-prong candidates in this collision
    nCand3 = rowTrackIndexProng3.lastIndex() - nCand3; // number of 3-prong candidates in this collision

    registry.fill(HIST("hNTracks"), nTracks);
    registry.fill(HIST("hNCand2Prong"), nCand2);
    registry.fill(HIST("hNCand3Prong"), nCand3);
    registry.fill(HIST("hNCand2ProngVsNTracks"), nTracks, nCand2);
    registry.fill(HIST("hNCand3ProngVsNTracks"), nTracks, nCand3);
  }
};

//________________________________________________________________________________________________________________________

/// Pre-selection of cascade secondary vertices
/// It will produce in any case a HfTrackIndexProng2 object, but mixing a V0
/// with a track, instead of 2 tracks

/// to run: o2-analysis-weak-decay-indices --aod-file AO2D.root -b | o2-analysis-lambdakzerobuilder -b |
///         o2-analysis-trackextension -b | o2-analysis-hf-track-index-skims-creator -b

struct HfTrackIndexSkimsCreatorCascades {
  Produces<aod::HfTrackIndexCasc> rowTrackIndexCasc;
  //  Produces<aod::HfTrackIndexProng2> rowTrackIndexCasc;

  // whether to do or not validation plots
  Configurable<bool> doValPlots{"doValPlots", true, "fill histograms"};

  // event selection
  //Configurable<int> triggerindex{"triggerindex", -1, "trigger index"};

  // vertexing parameters
  Configurable<double> bZ{"bZ", 5., "magnetic field"};
  Configurable<bool> propDCA{"propDCA", true, "create tracks version propagated to PCA"};
  Configurable<double> maxR{"maxR", 200., "reject PCA's above this radius"};
  Configurable<double> maxDZIni{"maxDZIni", 4., "reject (if>0) PCA candidate if tracks DZ exceeds threshold"};
  Configurable<double> minParamChange{"minParamChange", 1.e-3, "stop iterations if largest change of any X is smaller than this"};
  Configurable<double> minRelChi2Change{"minRelChi2Change", 0.9, "stop iterations if chi2/chi2old > this"};
  Configurable<bool> UseAbsDCA{"UseAbsDCA", true, "Use Abs DCAs"};

  // quality cut
  Configurable<bool> doCutQuality{"doCutQuality", true, "apply quality cuts"};

  // track cuts for bachelor
  Configurable<bool> TPCRefitBach{"TPCRefitBach", true, "request TPC refit bachelor"};
  Configurable<int> minCrossedRowsBach{"minCrossedRowsBach", 50, "min crossed rows bachelor"};

  // track cuts for V0 daughters
  Configurable<bool> TPCRefitV0Daugh{"TPCRefitV0Daugh", true, "request TPC refit V0 daughters"};
  Configurable<int> minCrossedRowsV0Daugh{"minCrossedRowsV0Daugh", 50, "min crossed rows V0 daughters"};

  // track cuts for V0 daughters
  Configurable<double> etaMax{"etaMax", 1.1, "max. pseudorapidity V0 daughters"};
  Configurable<double> ptMin{"ptMin", 0.05, "min. pT V0 daughters"};

  // bachelor cuts
  //  Configurable<float> dcabachtopv{"dcabachtopv", .1, "DCA Bach To PV"};
  //  Configurable<double> ptminbach{"ptminbach", -1., "min. track pT bachelor"};

  // v0 cuts
  Configurable<double> cosPAV0{"cosPAV0", .995, "CosPA V0"};                 // as in the task that create the V0s
  Configurable<double> dcaXYNegToPV{"dcaXYNegToPV", .1, "DCA_XY Neg To PV"}; // check: in HF Run 2, it was 0 at filtering
  Configurable<double> dcaXYPosToPV{"dcaXYPosToPV", .1, "DCA_XY Pos To PV"}; // check: in HF Run 2, it was 0 at filtering
  Configurable<double> cutInvMassV0{"cutInvMassV0", 0.05, "V0 candidate invariant mass difference wrt PDG"};

  // cascade cuts
  Configurable<double> cutCascPtCandMin{"cutCascPtCandMin", -1., "min. pT of the cascade candidate"};              // PbPb 2018: use 1
  Configurable<double> cutCascInvMassLc{"cutCascInvMassLc", 1., "Lc candidate invariant mass difference wrt PDG"}; // for PbPb 2018: use 0.2
  //Configurable<double> cutCascDCADaughters{"cutCascDCADaughters", .1, "DCA between V0 and bachelor in cascade"};

  // for debugging
#ifdef MY_DEBUG
  Configurable<std::vector<int>> indexK0Spos{"indexK0Spos", {729, 2866, 4754, 5457, 6891, 7824, 9243, 9810}, "indices of K0S positive daughters, for debug"};
  Configurable<std::vector<int>> indexK0Sneg{"indexK0Sneg", {730, 2867, 4755, 5458, 6892, 7825, 9244, 9811}, "indices of K0S negative daughters, for debug"};
  Configurable<std::vector<int>> indexProton{"indexProton", {717, 2810, 4393, 5442, 6769, 7793, 9002, 9789}, "indices of protons, for debug"};
#endif

  // histograms
  HistogramRegistry registry{
    "registry",
    {{"hVtx2ProngX", "2-prong candidates;#it{x}_{sec. vtx.} (cm);entries", {HistType::kTH1F, {{1000, -2., 2.}}}},
     {"hVtx2ProngY", "2-prong candidates;#it{y}_{sec. vtx.} (cm);entries", {HistType::kTH1F, {{1000, -2., 2.}}}},
     {"hVtx2ProngZ", "2-prong candidates;#it{z}_{sec. vtx.} (cm);entries", {HistType::kTH1F, {{1000, -20., 20.}}}},
     {"hmass2", "2-prong candidates;inv. mass (K0s p) (GeV/#it{c}^{2});entries", {HistType::kTH1F, {{500, 0., 5.}}}}}};

  // NB: using FullTracks = soa::Join<Tracks, TracksCov, TracksExtra>; defined in Framework/Core/include/Framework/AnalysisDataModel.h
  //using MyTracks = aod::BigTracksMC;
  //Partition<MyTracks> selectedTracks = aod::hf_seltrack::isSelProng >= 4;
  // using SelectedV0s = soa::Filtered<aod::V0Datas>;

  double massP = RecoDecay::getMassPDG(kProton);
  double massK0s = RecoDecay::getMassPDG(kK0Short);
  double massPi = RecoDecay::getMassPDG(kPiPlus);
  double massLc = RecoDecay::getMassPDG(pdg::Code::kLambdaCPlus);
  double mass2K0sP{0.}; // WHY HERE?

  Filter filterSelectCollisions = (aod::hf_selcollision::whyRejectColl == 0);

  using SelectedCollisions = soa::Filtered<soa::Join<aod::Collisions, aod::HFSelCollision>>;
  using FullTracksExt = soa::Join<aod::FullTracks, aod::TracksExtended>;

  void process(SelectedCollisions::iterator const& collision,
               aod::BCs const& bcs,
               //soa::Filtered<aod::V0Datas> const& V0s,
               aod::V0Datas const& V0s,
               MyTracks const& tracks
#ifdef MY_DEBUG
               ,
               aod::McParticles& mcParticles
#endif
               ) // TODO: I am now assuming that the V0s are already filtered with my cuts (David's work to come)
  {

    //Define o2 fitter, 2-prong
    o2::vertexing::DCAFitterN<2> fitter;
    fitter.setBz(bZ);
    fitter.setPropagateToPCA(propDCA);
    fitter.setMaxR(maxR);
    fitter.setMinParamChange(minParamChange);
    fitter.setMinRelChi2Change(minRelChi2Change);
    //fitter.setMaxDZIni(1e9); // used in cascadeproducer.cxx, but not for the 2 prongs
    //fitter.setMaxChi2(1e9);  // used in cascadeproducer.cxx, but not for the 2 prongs
    fitter.setUseAbsDCA(UseAbsDCA);

    // fist we loop over the bachelor candidate

    //for (const auto& bach : selectedTracks) {
    for (const auto& bach : tracks) {

      MY_DEBUG_MSG(1, printf("\n"); LOG(info) << "Bachelor loop");
#ifdef MY_DEBUG
      auto indexBach = bach.mcParticleId();
      bool isProtonFromLc = isProtonFromLcFunc(indexBach, indexProton);
#endif
      // selections on the bachelor
      // pT cut
      if (bach.isSelProng() < 4) {
        MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "proton " << indexBach << ": rejected due to HFsel");
        continue;
      }

      if (TPCRefitBach) {
        if (!(bach.trackType() & o2::aod::track::TPCrefit)) {
          MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "proton " << indexBach << ": rejected due to TPCrefit");
          continue;
        }
      }
      if (bach.tpcNClsCrossedRows() < minCrossedRowsBach) {
        MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "proton " << indexBach << ": rejected due to minNUmberOfCrossedRows " << bach.tpcNClsCrossedRows() << " (cut " << minCrossedRowsBach << ")");
        continue;
      }
      MY_DEBUG_MSG(isProtonFromLc, LOG(info) << "KEPT! proton from Lc with daughters " << indexBach);

      auto trackBach = getTrackParCov(bach);
      // now we loop over the V0s
      for (const auto& v0 : V0s) {
        MY_DEBUG_MSG(1, LOG(info) << "*** Checking next K0S");
        // selections on the V0 daughters
        const auto& trackV0DaughPos = v0.posTrack_as<MyTracks>();
        const auto& trackV0DaughNeg = v0.negTrack_as<MyTracks>();
#ifdef MY_DEBUG
        auto indexV0DaughPos = trackV0DaughPos.mcParticleId();
        auto indexV0DaughNeg = trackV0DaughNeg.mcParticleId();
        bool isK0SfromLc = isK0SfromLcFunc(indexV0DaughPos, indexV0DaughNeg, indexK0Spos, indexK0Sneg);

        bool isLc = isLcK0SpFunc(indexBach, indexV0DaughPos, indexV0DaughNeg, indexProton, indexK0Spos, indexK0Sneg);
#endif
        MY_DEBUG_MSG(isK0SfromLc, LOG(info) << "K0S from Lc found, trackV0DaughPos --> " << indexV0DaughPos << ", trackV0DaughNeg --> " << indexV0DaughNeg);

        MY_DEBUG_MSG(isK0SfromLc && isProtonFromLc,
                     LOG(info) << "ACCEPTED!!!";
                     LOG(info) << "proton belonging to a Lc found: label --> " << indexBach;
                     LOG(info) << "K0S belonging to a Lc found: trackV0DaughPos --> " << indexV0DaughPos << ", trackV0DaughNeg --> " << indexV0DaughNeg);

        MY_DEBUG_MSG(isLc, LOG(info) << "Combination of K0S and p which correspond to a Lc found!");

        if (TPCRefitV0Daugh) {
          if (!(trackV0DaughPos.trackType() & o2::aod::track::TPCrefit) ||
              !(trackV0DaughNeg.trackType() & o2::aod::track::TPCrefit)) {
            MY_DEBUG_MSG(isK0SfromLc, LOG(info) << "K0S with daughters " << indexV0DaughPos << " and " << indexV0DaughNeg << ": rejected due to TPCrefit");
            continue;
          }
        }
        if (trackV0DaughPos.tpcNClsCrossedRows() < minCrossedRowsV0Daugh ||
            trackV0DaughNeg.tpcNClsCrossedRows() < minCrossedRowsV0Daugh) {
          MY_DEBUG_MSG(isK0SfromLc, LOG(info) << "K0S with daughters " << indexV0DaughPos << " and " << indexV0DaughNeg << ": rejected due to minCrossedRows");
          continue;
        }
        //
        // if (trackV0DaughPos.dcaXY() < dcaXYPosToPV ||   // to the filters?
        //     trackV0DaughNeg.dcaXY() < dcaXYNegToPV) {
        //   continue;
        // }
        //
        if (trackV0DaughPos.pt() < ptMin || // to the filters? I can't for now, it is not in the tables
            trackV0DaughNeg.pt() < ptMin) {
          MY_DEBUG_MSG(isK0SfromLc, LOG(info) << "K0S with daughters " << indexV0DaughPos << " and " << indexV0DaughNeg << ": rejected due to minPt --> pos " << trackV0DaughPos.pt() << ", neg " << trackV0DaughNeg.pt() << " (cut " << ptMin << ")");
          continue;
        }
        if (std::abs(trackV0DaughPos.eta()) > etaMax || // to the filters? I can't for now, it is not in the tables
            std::abs(trackV0DaughNeg.eta()) > etaMax) {
          MY_DEBUG_MSG(isK0SfromLc, LOG(info) << "K0S with daughters " << indexV0DaughPos << " and " << indexV0DaughNeg << ": rejected due to eta --> pos " << trackV0DaughPos.eta() << ", neg " << trackV0DaughNeg.eta() << " (cut " << etaMax << ")");
          continue;
        }

        // V0 invariant mass selection
        if (std::abs(v0.mK0Short() - massK0s) > cutInvMassV0) {
          MY_DEBUG_MSG(isK0SfromLc, LOG(info) << "K0S with daughters " << indexV0DaughPos << " and " << indexV0DaughNeg << ": rejected due to invMass --> " << v0.mK0Short() - massK0s << " (cut " << cutInvMassV0 << ")");
          continue; // should go to the filter, but since it is a dynamic column, I cannot use it there
        }

        // V0 cosPointingAngle selection
        if (v0.v0cosPA(collision.posX(), collision.posY(), collision.posZ()) < cosPAV0) {
          MY_DEBUG_MSG(isK0SfromLc, LOG(info) << "K0S with daughters " << indexV0DaughPos << " and " << indexV0DaughNeg << ": rejected due to cosPA --> " << v0.v0cosPA(collision.posX(), collision.posY(), collision.posZ()) << " (cut " << cosPAV0 << ")");
          continue;
        }

        const std::array<float, 3> momentumV0 = {v0.px(), v0.py(), v0.pz()};

        // invariant-mass cut: we do it here, before updating the momenta of bach and V0 during the fitting to save CPU
        // TODO: but one should better check that the value here and after the fitter do not change significantly!!!
        mass2K0sP = RecoDecay::M(array{array{bach.px(), bach.py(), bach.pz()}, momentumV0}, array{massP, massK0s});
        if ((cutCascInvMassLc >= 0.) && (std::abs(mass2K0sP - massLc) > cutCascInvMassLc)) {
          MY_DEBUG_MSG(isK0SfromLc && isProtonFromLc, LOG(info) << "True Lc from proton " << indexBach << " and K0S pos " << indexV0DaughPos << " and neg " << indexV0DaughNeg << " rejected due to invMass cut: " << mass2K0sP << ", mass Lc " << massLc << " (cut " << cutCascInvMassLc << ")");
          continue;
        }

        MY_DEBUG_MSG(isK0SfromLc, LOG(info) << "KEPT! K0S from Lc with daughters " << indexV0DaughPos << " and " << indexV0DaughNeg);

        auto trackParCovV0DaughPos = getTrackParCov(trackV0DaughPos);
        trackParCovV0DaughPos.propagateTo(v0.posX(), bZ); // propagate the track to the X closest to the V0 vertex
        auto trackParCovV0DaughNeg = getTrackParCov(trackV0DaughNeg);
        trackParCovV0DaughNeg.propagateTo(v0.negX(), bZ); // propagate the track to the X closest to the V0 vertex
        std::array<float, 3> pVecV0 = {0., 0., 0.};
        std::array<float, 3> pVecBach = {0., 0., 0.};

        const std::array<float, 3> vertexV0 = {v0.x(), v0.y(), v0.z()};
        // we build the neutral track to then build the cascade
        auto trackV0 = o2::dataformats::V0(vertexV0, momentumV0, {0, 0, 0, 0, 0, 0}, trackParCovV0DaughPos, trackParCovV0DaughNeg, {0, 0}, {0, 0}); // build the V0 track

        // now we find the DCA between the V0 and the bachelor, for the cascade
        int nCand2 = fitter.process(trackV0, trackBach);
        MY_DEBUG_MSG(isK0SfromLc && isProtonFromLc, LOG(info) << "Fitter result = " << nCand2 << " proton = " << indexBach << " and K0S pos " << indexV0DaughPos << " and neg " << indexV0DaughNeg);
        MY_DEBUG_MSG(isLc, LOG(info) << "Fitter result for true Lc = " << nCand2);
        if (nCand2 == 0) {
          continue;
        }
        fitter.propagateTracksToVertex();        // propagate the bach and V0 to the Lc vertex
        fitter.getTrack(0).getPxPyPzGlo(pVecV0); // take the momentum at the Lc vertex
        fitter.getTrack(1).getPxPyPzGlo(pVecBach);

        // cascade candidate pT cut
        auto ptCascCand = RecoDecay::Pt(pVecBach, pVecV0);
        if (ptCascCand < cutCascPtCandMin) {
          MY_DEBUG_MSG(isK0SfromLc && isProtonFromLc, LOG(info) << "True Lc from proton " << indexBach << " and K0S pos " << indexV0DaughPos << " and neg " << indexV0DaughNeg << " rejected due to pt cut: " << ptCascCand << " (cut " << cutCascPtCandMin << ")");
          continue;
        }

        // invariant mass
        // re-calculate invariant masses with updated momenta, to fill the histogram
        mass2K0sP = RecoDecay::M(array{pVecBach, pVecV0}, array{massP, massK0s});

        std::array<float, 3> posCasc = {0., 0., 0.};
        const auto& cascVtx = fitter.getPCACandidate();
        for (int i = 0; i < 3; i++) {
          posCasc[i] = cascVtx[i];
        }

        // fill table row
        rowTrackIndexCasc(bach.globalIndex(),
                          v0.globalIndex(),
                          1); // 1 should be the value for the Lc
        // fill histograms
        if (doValPlots) {
          MY_DEBUG_MSG(isK0SfromLc && isProtonFromLc && isLc, LOG(info) << "KEPT! True Lc from proton " << indexBach << " and K0S pos " << indexV0DaughPos << " and neg " << indexV0DaughNeg);
          registry.fill(HIST("hVtx2ProngX"), posCasc[0]);
          registry.fill(HIST("hVtx2ProngY"), posCasc[1]);
          registry.fill(HIST("hVtx2ProngZ"), posCasc[2]);
          registry.fill(HIST("hmass2"), mass2K0sP);
        }

      } // loop over V0s

    } // loop over tracks
  }   // process
};

//________________________________________________________________________________________________________________________
WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  WorkflowSpec workflow{};

  const bool doTrigSel = cfgc.options().get<bool>("doTrigSel");
  if (doTrigSel) {
    workflow.push_back(adaptAnalysisTask<HfTagSelCollisions>(cfgc));
  } else {
    workflow.push_back(adaptAnalysisTask<HfTagSelCollisions>(cfgc, SetDefaultProcesses{{{"processTrigSel", false}, {"processNoTrigSel", true}}}));
  }

  workflow.push_back(adaptAnalysisTask<HfTagSelTracks>(cfgc));
  workflow.push_back(adaptAnalysisTask<HfTrackIndexSkimsCreator>(cfgc));

  const bool doCascades = cfgc.options().get<bool>("doCascades");
  if (doCascades) {
    workflow.push_back(adaptAnalysisTask<HfTrackIndexSkimsCreatorCascades>(cfgc));
  }

  return workflow;
}
