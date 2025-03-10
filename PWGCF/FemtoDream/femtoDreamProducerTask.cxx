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

/// \file femtoDreamProducerTask.cxx
/// \brief Tasks that produces the track tables used for the pairing
/// \author Andi Mathis, TU München, andreas.mathis@ph.tum.de

#include "FemtoDreamCollisionSelection.h"
#include "FemtoDreamTrackSelection.h"
#include "FemtoDreamV0Selection.h"
#include "PWGCF/DataModel/FemtoDerived.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/AnalysisTask.h"
#include "Framework/runDataProcessing.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/ASoAHelpers.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/Core/PID/PIDResponse.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/Multiplicity.h"
#include "ReconstructionDataFormats/Track.h"
#include "Common/Core/trackUtilities.h"
#include "Common/DataModel/StrangenessTables.h"
#include "Math/Vector4D.h"
#include "TMath.h"

using namespace o2;
using namespace o2::analysis::femtoDream;
using namespace o2::framework;
using namespace o2::framework::expressions;

namespace o2::aod
{

using FilteredFullCollision = soa::Filtered<soa::Join<aod::Collisions,
                                                      aod::EvSels,
                                                      aod::Mults>>::iterator;
using FilteredFullTracks = soa::Join<aod::FullTracks,
                                     aod::TracksExtended, aod::TOFSignal,
                                     aod::pidTPCEl, aod::pidTPCMu, aod::pidTPCPi,
                                     aod::pidTPCKa, aod::pidTPCPr, aod::pidTPCDe,
                                     aod::pidTOFEl, aod::pidTOFMu, aod::pidTOFPi,
                                     aod::pidTOFKa, aod::pidTOFPr, aod::pidTOFDe>;
// using FilteredFullV0s = soa::Filtered<aod::V0Datas>; /// predefined Join table for o2::aod::V0s = soa::Join<o2::aod::TransientV0s, o2::aod::StoredV0s> to be used when we add v0Filter
} // namespace o2::aod

/// \todo fix how to pass array to setSelection, getRow() passing a different type!
// static constexpr float arrayV0Sel[3][3] = {{100.f, 100.f, 100.f}, {0.2f, 0.2f, 0.2f}, {100.f, 100.f, 100.f}};
// unsigned int rows = sizeof(arrayV0Sel) / sizeof(arrayV0Sel[0]);
// unsigned int columns = sizeof(arrayV0Sel[0]) / sizeof(arrayV0Sel[0][0]);

template <typename T>
int getRowDaughters(int daughID, T const& vecID)
{
  int rowInPrimaryTrackTableDaugh = -1;
  for (size_t i = 0; i < vecID.size(); i++) {
    if (vecID.at(i) == daughID) {
      rowInPrimaryTrackTableDaugh = i;
      break;
    }
  }
  return rowInPrimaryTrackTableDaugh;
}

struct femtoDreamProducerTask {

  Produces<aod::FemtoDreamCollisions> outputCollision;
  Produces<aod::FemtoDreamParticles> outputTracks;
  Produces<aod::FemtoDreamDebugParticles> outputDebugTracks;

  Configurable<bool> ConfDebugOutput{"ConfDebugOutput", true, "Debug output"};

  // Choose if filtering or skimming version is run

  Configurable<bool> ConfIsTrigger{"ConfIsTrigger", false, "Store all collisions"};

  /// Event cuts
  FemtoDreamCollisionSelection colCuts;
  Configurable<float> ConfEvtZvtx{"ConfEvtZvtx", 10.f, "Evt sel: Max. z-Vertex (cm)"};
  Configurable<bool> ConfEvtTriggerCheck{"ConfEvtTriggerCheck", true, "Evt sel: check for trigger"};
  Configurable<int> ConfEvtTriggerSel{"ConfEvtTriggerSel", kINT7, "Evt sel: trigger"};
  Configurable<bool> ConfEvtOfflineCheck{"ConfEvtOfflineCheck", false, "Evt sel: check for offline selection"};

  Filter colFilter = (ConfIsTrigger == (uint8_t) true) || (nabs(aod::collision::posZ) < ConfEvtZvtx);

  FemtoDreamTrackSelection trackCuts;
  Configurable<std::vector<float>> ConfTrkCharge{FemtoDreamTrackSelection::getSelectionName(femtoDreamTrackSelection::kSign, "ConfTrk"), std::vector<float>{-1, 1}, FemtoDreamTrackSelection::getSelectionHelper(femtoDreamTrackSelection::kSign, "Track selection: ")};
  Configurable<std::vector<float>> ConfTrkPtmin{FemtoDreamTrackSelection::getSelectionName(femtoDreamTrackSelection::kpTMin, "ConfTrk"), std::vector<float>{0.4f, 0.6f, 0.5f}, FemtoDreamTrackSelection::getSelectionHelper(femtoDreamTrackSelection::kpTMin, "Track selection: ")};
  Configurable<std::vector<float>> ConfTrkEta{FemtoDreamTrackSelection::getSelectionName(femtoDreamTrackSelection::kEtaMax, "ConfTrk"), std::vector<float>{0.8f, 0.7f, 0.9f}, FemtoDreamTrackSelection::getSelectionHelper(femtoDreamTrackSelection::kEtaMax, "Track selection: ")};
  Configurable<std::vector<float>> ConfTrkTPCnclsMin{FemtoDreamTrackSelection::getSelectionName(femtoDreamTrackSelection::kTPCnClsMin, "ConfTrk"), std::vector<float>{80.f, 70.f, 60.f}, FemtoDreamTrackSelection::getSelectionHelper(femtoDreamTrackSelection::kTPCnClsMin, "Track selection: ")};
  Configurable<std::vector<float>> ConfTrkTPCfCls{FemtoDreamTrackSelection::getSelectionName(femtoDreamTrackSelection::kTPCfClsMin, "ConfTrk"), std::vector<float>{0.7f, 0.83f, 0.9f}, FemtoDreamTrackSelection::getSelectionHelper(femtoDreamTrackSelection::kTPCfClsMin, "Track selection: ")};
  Configurable<std::vector<float>> ConfTrkTPCcRowsMin{FemtoDreamTrackSelection::getSelectionName(femtoDreamTrackSelection::kTPCcRowsMin, "ConfTrk"), std::vector<float>{70.f, 60.f, 80.f}, FemtoDreamTrackSelection::getSelectionHelper(femtoDreamTrackSelection::kTPCcRowsMin, "Track selection: ")};
  Configurable<std::vector<float>> ConfTrkTPCsCls{FemtoDreamTrackSelection::getSelectionName(femtoDreamTrackSelection::kTPCsClsMax, "ConfTrk"), std::vector<float>{0.1f, 160.f}, FemtoDreamTrackSelection::getSelectionHelper(femtoDreamTrackSelection::kTPCsClsMax, "Track selection: ")};
  Configurable<std::vector<float>> ConfTrkDCAxyMax{FemtoDreamTrackSelection::getSelectionName(femtoDreamTrackSelection::kDCAxyMax, "ConfTrk"), std::vector<float>{0.1f, 3.5f}, FemtoDreamTrackSelection::getSelectionHelper(femtoDreamTrackSelection::kDCAxyMax, "Track selection: ")}; /// here we need an open cut to do the DCA fits later on!
  Configurable<std::vector<float>> ConfTrkDCAzMax{FemtoDreamTrackSelection::getSelectionName(femtoDreamTrackSelection::kDCAzMax, "ConfTrk"), std::vector<float>{0.2f, 3.5f}, FemtoDreamTrackSelection::getSelectionHelper(femtoDreamTrackSelection::kDCAzMax, "Track selection: ")};
  /// \todo Reintegrate PID to the general selection container
  Configurable<std::vector<float>> ConfTrkPIDnSigmaMax{FemtoDreamTrackSelection::getSelectionName(femtoDreamTrackSelection::kPIDnSigmaMax, "ConfTrk"), std::vector<float>{3.5f, 3.f, 2.5f}, FemtoDreamTrackSelection::getSelectionHelper(femtoDreamTrackSelection::kPIDnSigmaMax, "Track selection: ")};
  Configurable<std::vector<int>> ConfTrkTPIDspecies{"ConfTrkTPIDspecies", std::vector<int>{o2::track::PID::Pion, o2::track::PID::Kaon, o2::track::PID::Proton, o2::track::PID::Deuteron}, "Trk sel: Particles species for PID"};

  FemtoDreamV0Selection v0Cuts;
  /// \todo Labeled array (see Track-Track task)

  Configurable<std::vector<float>> ConfV0Sign{FemtoDreamV0Selection::getSelectionName(femtoDreamV0Selection::kV0Sign, "ConfV0"), std::vector<float>{-1, 1}, FemtoDreamV0Selection::getSelectionHelper(femtoDreamV0Selection::kV0Sign, "V0 selection: ")};
  Configurable<std::vector<float>> ConfV0PtMin{FemtoDreamV0Selection::getSelectionName(femtoDreamV0Selection::kpTV0Min, "ConfV0"), std::vector<float>{0.3f, 0.4f, 0.5f}, FemtoDreamV0Selection::getSelectionHelper(femtoDreamV0Selection::kpTV0Min, "V0 selection: ")};
  Configurable<std::vector<float>> ConfDCAV0DaughMax{FemtoDreamV0Selection::getSelectionName(femtoDreamV0Selection::kDCAV0DaughMax, "ConfV0"), std::vector<float>{1.2f, 1.5f}, FemtoDreamV0Selection::getSelectionHelper(femtoDreamV0Selection::kDCAV0DaughMax, "V0 selection: ")};
  Configurable<std::vector<float>> ConfCPAV0Min{FemtoDreamV0Selection::getSelectionName(femtoDreamV0Selection::kCPAV0Min, "ConfV0"), std::vector<float>{0.9f, 0.995f}, FemtoDreamV0Selection::getSelectionHelper(femtoDreamV0Selection::kCPAV0Min, "V0 selection: ")};

  MutableConfigurable<float> V0TranRadV0Min{FemtoDreamV0Selection::getSelectionName(femtoDreamV0Selection::kTranRadV0Min, "ConfV0"), 0.2f, FemtoDreamV0Selection::getSelectionHelper(femtoDreamV0Selection::kTranRadV0Min, "V0 selection: ")}; // QUESTION: why mutable?
  MutableConfigurable<float> V0TranRadV0Max{FemtoDreamV0Selection::getSelectionName(femtoDreamV0Selection::kTranRadV0Max, "ConfV0"), 100.f, FemtoDreamV0Selection::getSelectionHelper(femtoDreamV0Selection::kTranRadV0Max, "V0 selection: ")};
  MutableConfigurable<float> V0DecVtxMax{FemtoDreamV0Selection::getSelectionName(femtoDreamV0Selection::kDecVtxMax, "ConfV0"), 100.f, FemtoDreamV0Selection::getSelectionHelper(femtoDreamV0Selection::kDecVtxMax, "V0 selection: ")};

  Configurable<std::vector<float>> ConfV0DaughTPCnclsMin{"ConfV0DaughTPCnclsMin", std::vector<float>{80.f, 70.f, 60.f}, "V0 Daugh sel: Min. nCls TPC"};
  Configurable<std::vector<float>> ConfV0DaughDCAMin{"ConfV0DaughDCAMin", std::vector<float>{0.05f, 0.06f}, "V0 Daugh sel:  Max. DCA Daugh to PV (cm)"};
  Configurable<std::vector<float>> ConfV0DaughPIDnSigmaMax{"ConfV0DaughPIDnSigmaMax", std::vector<float>{5.f, 4.f}, "V0 Daugh sel: Max. PID nSigma TPC"};

  /// \todo should we add filter on min value pT/eta of V0 and daughters?
  /*Filter v0Filter = (nabs(aod::v0data::x) < V0DecVtxMax.value) &&
                    (nabs(aod::v0data::y) < V0DecVtxMax.value) &&
                    (nabs(aod::v0data::z) < V0DecVtxMax.value);*/
  // (aod::v0data::v0radius > V0TranRadV0Min.value); to be added, not working for now do not know why

  HistogramRegistry qaRegistry{"QAHistos", {}, OutputObjHandlingPolicy::QAObject};

  void init(InitContext&)
  {
    colCuts.setCuts(ConfEvtZvtx, ConfEvtTriggerCheck, ConfEvtTriggerSel, ConfEvtOfflineCheck);
    colCuts.init(&qaRegistry);

    trackCuts.setSelection(ConfTrkCharge, femtoDreamTrackSelection::kSign, femtoDreamSelection::kEqual);
    trackCuts.setSelection(ConfTrkPtmin, femtoDreamTrackSelection::kpTMin, femtoDreamSelection::kLowerLimit);
    trackCuts.setSelection(ConfTrkEta, femtoDreamTrackSelection::kEtaMax, femtoDreamSelection::kAbsUpperLimit);
    trackCuts.setSelection(ConfTrkTPCnclsMin, femtoDreamTrackSelection::kTPCnClsMin, femtoDreamSelection::kLowerLimit);
    trackCuts.setSelection(ConfTrkTPCfCls, femtoDreamTrackSelection::kTPCfClsMin, femtoDreamSelection::kLowerLimit);
    trackCuts.setSelection(ConfTrkTPCcRowsMin, femtoDreamTrackSelection::kTPCcRowsMin, femtoDreamSelection::kLowerLimit);
    trackCuts.setSelection(ConfTrkTPCsCls, femtoDreamTrackSelection::kTPCsClsMax, femtoDreamSelection::kUpperLimit);
    trackCuts.setSelection(ConfTrkDCAxyMax, femtoDreamTrackSelection::kDCAxyMax, femtoDreamSelection::kAbsUpperLimit);
    trackCuts.setSelection(ConfTrkDCAzMax, femtoDreamTrackSelection::kDCAzMax, femtoDreamSelection::kAbsUpperLimit);
    trackCuts.setSelection(ConfTrkPIDnSigmaMax, femtoDreamTrackSelection::kPIDnSigmaMax, femtoDreamSelection::kAbsUpperLimit);
    trackCuts.setPIDSpecies(ConfTrkTPIDspecies);
    trackCuts.init<aod::femtodreamparticle::ParticleType::kTrack, aod::femtodreamparticle::cutContainerType>(&qaRegistry);

    /// \todo fix how to pass array to setSelection, getRow() passing a different type!
    // v0Cuts.setSelection(ConfV0Selection->getRow(0), femtoDreamV0Selection::kDecVtxMax, femtoDreamSelection::kAbsUpperLimit);
    v0Cuts.setSelection(ConfV0Sign, femtoDreamV0Selection::kV0Sign, femtoDreamSelection::kEqual);
    v0Cuts.setSelection(ConfV0PtMin, femtoDreamV0Selection::kpTV0Min, femtoDreamSelection::kLowerLimit);
    v0Cuts.setSelection(ConfDCAV0DaughMax, femtoDreamV0Selection::kDCAV0DaughMax, femtoDreamSelection::kUpperLimit);
    v0Cuts.setSelection(ConfCPAV0Min, femtoDreamV0Selection::kCPAV0Min, femtoDreamSelection::kLowerLimit);

    v0Cuts.setChildCuts(femtoDreamV0Selection::kPosTrack, ConfTrkCharge, femtoDreamTrackSelection::kSign, femtoDreamSelection::kEqual);
    v0Cuts.setChildCuts(femtoDreamV0Selection::kPosTrack, ConfTrkEta, femtoDreamTrackSelection::kEtaMax, femtoDreamSelection::kAbsUpperLimit);
    v0Cuts.setChildCuts(femtoDreamV0Selection::kPosTrack, ConfV0DaughTPCnclsMin, femtoDreamTrackSelection::kTPCnClsMin, femtoDreamSelection::kLowerLimit);
    // v0Cuts.setChildCuts(femtoDreamV0Selection::kPosTrack, ConfV0DaughDCAMin, femtoDreamTrackSelection::kDCAMin, femtoDreamSelection::kAbsLowerLimit);
    v0Cuts.setChildCuts(femtoDreamV0Selection::kPosTrack, ConfV0DaughPIDnSigmaMax, femtoDreamTrackSelection::kPIDnSigmaMax, femtoDreamSelection::kAbsUpperLimit);
    v0Cuts.setChildCuts(femtoDreamV0Selection::kNegTrack, ConfTrkCharge, femtoDreamTrackSelection::kSign, femtoDreamSelection::kEqual);
    v0Cuts.setChildCuts(femtoDreamV0Selection::kNegTrack, ConfTrkEta, femtoDreamTrackSelection::kEtaMax, femtoDreamSelection::kAbsUpperLimit);
    v0Cuts.setChildCuts(femtoDreamV0Selection::kNegTrack, ConfV0DaughTPCnclsMin, femtoDreamTrackSelection::kTPCnClsMin, femtoDreamSelection::kLowerLimit);
    // v0Cuts.setChildCuts(femtoDreamV0Selection::kNegTrack, ConfV0DaughDCAMin, femtoDreamTrackSelection::kDCAMin, femtoDreamSelection::kAbsLowerLimit);
    v0Cuts.setChildCuts(femtoDreamV0Selection::kNegTrack, ConfV0DaughPIDnSigmaMax, femtoDreamTrackSelection::kPIDnSigmaMax, femtoDreamSelection::kAbsUpperLimit);
    v0Cuts.setChildPIDSpecies(femtoDreamV0Selection::kPosTrack, ConfTrkTPIDspecies);
    v0Cuts.setChildPIDSpecies(femtoDreamV0Selection::kNegTrack, ConfTrkTPIDspecies);
    v0Cuts.init<aod::femtodreamparticle::ParticleType::kV0, aod::femtodreamparticle::ParticleType::kV0Child, aod::femtodreamparticle::cutContainerType>(&qaRegistry);
  }

  void process(aod::FilteredFullCollision const& col, aod::BCsWithTimestamps const&, aod::FilteredFullTracks const& tracks,
               o2::aod::V0Datas const& fullV0s) /// \todo with FilteredFullV0s
  {
    auto bc = col.bc_as<aod::BCsWithTimestamps>(); /// adding timestamp to access magnetic field later
    /// First thing to do is to check whether the basic event selection criteria are fulfilled
    // If the basic selection is NOT fullfilled:
    // in case of skimming run - don't store such collisions
    // in case of trigger run - store such collisions but don't store any particle candidates for such collisions
    if (!colCuts.isSelected(col)) {
      if (ConfIsTrigger) {
        outputCollision(col.posZ(), col.multV0M(), colCuts.computeSphericity(col, tracks), bc.timestamp());
      }
      return;
    }

    const auto vtxZ = col.posZ();
    const auto mult = col.multV0M();
    const auto spher = colCuts.computeSphericity(col, tracks);
    colCuts.fillQA(col);

    std::cout << "###########################" << std::endl;
    std::cout << "###########################" << std::endl;
    std::cout << "#########HELLO##################" << std::endl;
    std::cout << "timestamp = " << bc.timestamp() << std::endl;
    std::cout << "###########################" << std::endl;
    std::cout << "###########################" << std::endl;
    std::cout << "###########################" << std::endl;

    // now the table is filled
    outputCollision(vtxZ, mult, spher, bc.timestamp());

    int childIDs[2] = {0, 0};    // these IDs are necessary to keep track of the children
    std::vector<int> tmpIDtrack; // this vector keeps track of the matching of the primary track table row <-> aod::track table global index

    for (auto& track : tracks) {
      /// if the most open selection criteria are not fulfilled there is no point looking further at the track
      if (!trackCuts.isSelectedMinimal(track)) {
        continue;
      }
      trackCuts.fillQA<aod::femtodreamparticle::ParticleType::kTrack>(track);
      // the bit-wise container of the systematic variations is obtained
      auto cutContainer = trackCuts.getCutContainer<aod::femtodreamparticle::cutContainerType>(track);

      // now the table is filled
      outputTracks(outputCollision.lastIndex(), track.pt(), track.eta(), track.phi(), aod::femtodreamparticle::ParticleType::kTrack, cutContainer.at(femtoDreamTrackSelection::TrackContainerPosition::kCuts), cutContainer.at(femtoDreamTrackSelection::TrackContainerPosition::kPID), track.dcaXY(), childIDs);
      tmpIDtrack.push_back(track.globalIndex());
      if (ConfDebugOutput) {
        outputDebugTracks(outputCollision.lastIndex(),
                          track.sign(), track.tpcNClsFound(),
                          track.tpcNClsFindable(),
                          track.tpcNClsCrossedRows(), track.tpcNClsShared(), track.dcaXY(), track.dcaZ(),
                          track.tpcNSigmaEl(), track.tpcNSigmaPi(), track.tpcNSigmaKa(), track.tpcNSigmaPr(), track.tpcNSigmaDe(),
                          track.tofNSigmaEl(), track.tofNSigmaPi(), track.tofNSigmaKa(), track.tofNSigmaPr(), track.tofNSigmaDe());
      }
    }

    for (auto& v0 : fullV0s) {
      auto postrack = v0.posTrack_as<aod::FilteredFullTracks>();
      auto negtrack = v0.negTrack_as<aod::FilteredFullTracks>(); ///\tocheck funnily enough if we apply the filter the sign of Pos and Neg track is always negative
      // const auto dcaXYpos = postrack.dcaXY();
      // const auto dcaZpos = postrack.dcaZ();
      // const auto dcapos = std::sqrt(pow(dcaXYpos, 2.) + pow(dcaZpos, 2.));
      if (!v0Cuts.isSelectedMinimal(col, v0, postrack, negtrack)) {
        continue;
      }
      v0Cuts.fillQA<aod::femtodreamparticle::ParticleType::kV0, aod::femtodreamparticle::ParticleType::kV0Child>(col, v0, postrack, negtrack); ///\todo fill QA also for daughters
      auto cutContainerV0 = v0Cuts.getCutContainer<aod::femtodreamparticle::cutContainerType>(col, v0, postrack, negtrack);

      if ((cutContainerV0.at(femtoDreamV0Selection::V0ContainerPosition::kV0) > 0) && (cutContainerV0.at(femtoDreamV0Selection::V0ContainerPosition::kPosCuts) > 0) && (cutContainerV0.at(femtoDreamV0Selection::V0ContainerPosition::kNegCuts) > 0)) {
        int postrackID = v0.posTrackId();
        int rowInPrimaryTrackTablePos = -1;
        rowInPrimaryTrackTablePos = getRowDaughters(postrackID, tmpIDtrack);
        childIDs[0] = rowInPrimaryTrackTablePos;
        childIDs[1] = 0;
        outputTracks(outputCollision.lastIndex(), v0.positivept(), v0.positiveeta(), v0.positivephi(), aod::femtodreamparticle::ParticleType::kV0Child, cutContainerV0.at(femtoDreamV0Selection::V0ContainerPosition::kPosCuts), cutContainerV0.at(femtoDreamV0Selection::V0ContainerPosition::kPosPID), 0., childIDs);
        const int rowOfPosTrack = outputTracks.lastIndex();
        int negtrackID = v0.negTrackId();
        int rowInPrimaryTrackTableNeg = -1;
        rowInPrimaryTrackTableNeg = getRowDaughters(negtrackID, tmpIDtrack);
        childIDs[0] = 0;
        childIDs[1] = rowInPrimaryTrackTableNeg;
        outputTracks(outputCollision.lastIndex(), v0.negativept(), v0.negativeeta(), v0.negativephi(), aod::femtodreamparticle::ParticleType::kV0Child, cutContainerV0.at(femtoDreamV0Selection::V0ContainerPosition::kNegCuts), cutContainerV0.at(femtoDreamV0Selection::V0ContainerPosition::kNegPID), 0., childIDs);
        const int rowOfNegTrack = outputTracks.lastIndex();
        int indexChildID[2] = {rowOfPosTrack, rowOfNegTrack};
        outputTracks(outputCollision.lastIndex(), v0.pt(), v0.eta(), v0.phi(), aod::femtodreamparticle::ParticleType::kV0, cutContainerV0.at(femtoDreamV0Selection::V0ContainerPosition::kV0), 0, v0.v0cosPA(col.posX(), col.posY(), col.posZ()), indexChildID);
      }
    }
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  WorkflowSpec workflow{adaptAnalysisTask<femtoDreamProducerTask>(cfgc)};
  return workflow;
}
