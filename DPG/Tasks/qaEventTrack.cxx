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
/// \author Peter Hristov <Peter.Hristov@cern.ch>, CERN
/// \author Gian Michele Innocenti <gian.michele.innocenti@cern.ch>, CERN
/// \author Henrique J C Zanoli <henrique.zanoli@cern.ch>, Utrecht University
/// \author Nicolo' Jacazio <nicolo.jacazio@cern.ch>, CERN

#include "Framework/AnalysisTask.h"
#include "Framework/HistogramRegistry.h"
#include "ReconstructionDataFormats/DCA.h"
#include "Common/Core/trackUtilities.h"

#include "Framework/AnalysisDataModel.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Common/Core/TrackSelection.h"
#include "Common/Core/TrackSelectionDefaults.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;

void customize(std::vector<o2::framework::ConfigParamSpec>& workflowOptions)
{
  std::vector<ConfigParamSpec> options{
    {"impPar", VariantType::Int, 1, {"Include impact paramter studies."}}};
  std::swap(workflowOptions, options);
}

#include "Framework/runDataProcessing.h"

using namespace o2::framework;
using namespace o2::dataformats;

// TODO: add PID wagons as dependency + include impact parameter studies (same or separate task in workflow??)

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
// Task declaration
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct qaEventTrack {

  // general steering settings
  Configurable<bool> isMC{"isMC", true, "Is MC dataset"};        // TODO: derive this from metadata once possible to get rid of the flag
  Configurable<bool> isRun3{"isRun3", false, "Is Run3 dataset"}; // TODO: derive this from metadata once possible to get rid of the flag

  // options to select specific events
  Configurable<bool> selectGoodEvents{"selectGoodEvents", true, "select good events"};

  // options to select only specific tracks
  Configurable<bool> selectGlobalTracks{"selectGlobalTracks", true, "select global tracks"};
  Configurable<int> selectCharge{"selectCharge", 0, "select charge +1 or -1 (0 means no selection)"};
  Configurable<bool> selectPrim{"selectPrim", false, "select primaries"};
  Configurable<bool> selectSec{"selectSec", false, "select secondaries"};
  Configurable<int> selectPID{"selectPID", 0, "select pid"};

  // configurable binning of histograms
  ConfigurableAxis binsPt{"binsPt", {VARIABLE_WIDTH, 0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 2.0, 5.0, 10.0, 20.0, 50.0}, ""};

  ConfigurableAxis binsVertexPosZ{"binsVertexPosZ", {100, -20., 20.}, ""}; // TODO: do we need this to be configurable?
  ConfigurableAxis binsVertexPosXY{"binsVertexPosXY", {500, -1., 1.}, ""}; // TODO: do we need this to be configurable?
  ConfigurableAxis binsTrackMultiplicity{"binsTrackMultiplcity", {200, 0, 200}, ""};

  // TODO: ask if one can have different filters for both process functions
  Filter trackFilter = !selectGlobalTracks || aod::track::isGlobalTrack == (uint8_t) true;

  HistogramRegistry histos{"histos", {}, OutputObjHandlingPolicy::QAObject};

  void init(InitContext const&);

  template <bool IS_MC, typename T>
  bool isSelectedTrack(const T& track);

  using CollisionTableData = soa::Join<aod::Collisions, aod::EvSels>;
  using TrackTableData = soa::Filtered<soa::Join<aod::FullTracks, aod::TracksExtended, aod::TrackSelection>>;
  void processData(CollisionTableData::iterator const& collision, TrackTableData const& tracks)
  {
    processReco<false>(collision, tracks);
  };
  PROCESS_SWITCH(qaEventTrack, processData, "process data", false);

  using CollisionTableMC = soa::Join<aod::Collisions, aod::McCollisionLabels, aod::EvSels>;
  using TrackTableMC = soa::Filtered<soa::Join<aod::FullTracks, aod::McTrackLabels, aod::TracksExtended, aod::TrackSelection>>;
  void processMC(CollisionTableMC::iterator const& collision, TrackTableMC const& tracks, aod::McParticles const& mcParticles, aod::McCollisions const& mcCollisions)
  {
    processReco<true>(collision, tracks);
  };
  PROCESS_SWITCH(qaEventTrack, processMC, "process mc", true); // FIXME: would like to disable this by default and swich on via --processMC but currently this crashes -> ask experts

  template <bool IS_MC, typename C, typename T>
  void processReco(const C& collision, const T& tracks);
};
WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  WorkflowSpec workflow;
  workflow.push_back(adaptAnalysisTask<qaEventTrack>(cfgc));
  // if (cfgc.options().get<int>("impPar")) {}
  return workflow;
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
// Task implementation
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

//**************************************************************************************************
/**
 * Initialize the task.
 */
//**************************************************************************************************
void qaEventTrack::init(InitContext const&)
{
  const AxisSpec axisPt{binsPt, "#it{p}_{T} [GeV/c]"};
  const AxisSpec axisVertexNumContrib{200, 0, 200, "Number Of contributors to the PV"};
  const AxisSpec axisVertexPosX{binsVertexPosXY, "X [cm]"};
  const AxisSpec axisVertexPosY{binsVertexPosXY, "Y [cm]"};
  const AxisSpec axisVertexPosZ{binsVertexPosZ, "Z [cm]"};
  const AxisSpec axisVertexCov{100, -0.005, 0.005};
  const AxisSpec axisVertexPosReso{100, -0.5, 0.5};
  const AxisSpec axisTrackMultiplicity{binsTrackMultiplicity, "Track Multiplicity"};

  const AxisSpec axisDeltaPt{100, -0.5, 0.5, "#it{p}_{T, rec} - #it{p}_{T, gen}"};
  const AxisSpec axisDeltaEta{100, -0.1, 0.1, "#eta_{rec} - #eta_{gen}"};
  const AxisSpec axisDeltaPhi{100, -0.1, 0.1, "#phi_{rec} - #phi_{gen}"};

  // collision
  auto eventRecoEffHist = histos.add<TH1>("Events/recoEff", "", kTH1D, {{2, 0.5, 2.5}});
  eventRecoEffHist->GetXaxis()->SetBinLabel(1, "all");
  eventRecoEffHist->GetXaxis()->SetBinLabel(2, "selected");
  histos.add("Events/posX", "", kTH1D, {axisVertexPosX});
  histos.add("Events/posY", "", kTH1D, {axisVertexPosY});
  histos.add("Events/posZ", "", kTH1D, {axisVertexPosZ});
  histos.add("Events/posXY", "", kTH2D, {axisVertexPosX, axisVertexPosY});
  histos.add("Events/posXvsNContrib", "", kTH2D, {axisVertexPosX, axisVertexNumContrib});
  histos.add("Events/posYvsNContrib", "", kTH2D, {axisVertexPosY, axisVertexNumContrib});
  histos.add("Events/posZvsNContrib", "", kTH2D, {axisVertexPosZ, axisVertexNumContrib});
  histos.add("Events/nContrib", "", kTH1D, {axisVertexNumContrib});
  histos.add("Events/nContribVsMult", "", kTH2D, {axisVertexNumContrib, axisTrackMultiplicity});
  histos.add("Events/vertexChi2", ";#chi^{2}", kTH1D, {{100, 0, 100}});

  histos.add("Events/covXX", ";Cov_{xx} [cm^{2}]", kTH1D, {axisVertexCov});
  histos.add("Events/covXY", ";Cov_{xy} [cm^{2}]", kTH1D, {axisVertexCov});
  histos.add("Events/covXZ", ";Cov_{xz} [cm^{2}]", kTH1D, {axisVertexCov});
  histos.add("Events/covYY", ";Cov_{yy} [cm^{2}]", kTH1D, {axisVertexCov});
  histos.add("Events/covYZ", ";Cov_{yz} [cm^{2}]", kTH1D, {axisVertexCov});
  histos.add("Events/covZZ", ";Cov_{zz} [cm^{2}]", kTH1D, {axisVertexCov});

  histos.add("Events/nTracks", "", kTH1D, {axisTrackMultiplicity});

  if (isMC) {
    histos.add("Events/resoX", ";X_{Rec} - X_{Gen} [cm]", kTH2D, {axisVertexPosReso, axisVertexNumContrib});
    histos.add("Events/resoY", ";Y_{Rec} - Y_{Gen} [cm]", kTH2D, {axisVertexPosReso, axisVertexNumContrib});
    histos.add("Events/resoZ", ";Z_{Rec} - Z_{Gen} [cm]", kTH2D, {axisVertexPosReso, axisVertexNumContrib});
  }

  auto trackRecoEffHist = histos.add<TH1>("Tracks/recoEff", "", kTH1D, {{2, 0.5, 2.5}});
  trackRecoEffHist->GetXaxis()->SetBinLabel(1, "all");
  trackRecoEffHist->GetXaxis()->SetBinLabel(2, "selected");
  trackRecoEffHist->SetBit(TH1::kIsNotW);

  // kine histograms
  histos.add("Tracks/Kine/pt", "#it{p}_{T};#it{p}_{T} [GeV/c]", kTH1D, {{axisPt}});
  histos.add("Tracks/Kine/eta", "#eta;#eta", kTH1D, {{180, -0.9, 0.9}});
  histos.add("Tracks/Kine/phi", "#phi;#phi [rad]", kTH1D, {{180, 0., 2 * M_PI}});
  if (isMC) {
    histos.add("Tracks/Kine/resoPt", "", kTH2D, {axisDeltaPt, axisPt});
    histos.add("Tracks/Kine/resoEta", "", kTH2D, {axisDeltaEta, {180, -0.9, 0.9, "#eta_{rec}"}});
    histos.add("Tracks/Kine/resoPhi", "", kTH2D, {axisDeltaPhi, {180, 0., 2 * M_PI, "#phi_{rec}"}});
  }
  // track histograms
  histos.add("Tracks/x", "track #it{x} position at dca in local coordinate system;#it{x} [cm]", kTH1D, {{200, -0.36, 0.36}});
  histos.add("Tracks/y", "track #it{y} position at dca in local coordinate system;#it{y} [cm]", kTH1D, {{200, -0.5, 0.5}});
  histos.add("Tracks/z", "track #it{z} position at dca in local coordinate system;#it{z} [cm]", kTH1D, {{200, -11., 11.}});
  histos.add("Tracks/alpha", "rotation angle of local wrt. global coordinate system;#alpha [rad]", kTH1D, {{36, -M_PI, M_PI}});
  histos.add("Tracks/signed1Pt", "track signed 1/#it{p}_{T};#it{q}/#it{p}_{T}", kTH1D, {{200, -8, 8}});
  histos.add("Tracks/snp", "sinus of track momentum azimuthal angle;snp", kTH1D, {{11, -0.1, 0.1}});
  histos.add("Tracks/tgl", "tangent of the track momentum dip angle;tgl;", kTH1D, {{200, -1., 1.}});
  histos.add("Tracks/flags", "track flag;flag bit", kTH1D, {{64, -0.5, 63.5}});
  histos.add("Tracks/dcaXY", "distance of closest approach in #it{xy} plane;#it{dcaXY} [cm];", kTH1D, {{200, -0.15, 0.15}});
  histos.add("Tracks/dcaZ", "distance of closest approach in #it{z};#it{dcaZ} [cm];", kTH1D, {{200, -0.15, 0.15}});

  histos.add("Tracks/dcaXYvsPt", "distance of closest approach in #it{xy} plane;#it{dcaXY} [cm];", kTH2D, {{200, -0.15, 0.15}, axisPt});
  histos.add("Tracks/dcaZvsPt", "distance of closest approach in #it{z};#it{dcaZ} [cm];", kTH2D, {{200, -0.15, 0.15}, axisPt});

  histos.add("Tracks/length", "track length in cm;#it{Length} [cm];", kTH1D, {{400, 0, 1000}}); // FIXME: why is this zero most of the time?

  // its histograms
  histos.add("Tracks/ITS/itsNCls", "number of found ITS clusters;# clusters ITS", kTH1D, {{8, -0.5, 7.5}});
  histos.add("Tracks/ITS/itsChi2NCl", "chi2 per ITS cluster;chi2 / cluster ITS", kTH1D, {{100, 0, 40}});
  histos.add("Tracks/ITS/itsHits", "hitmap ITS;layer ITS", kTH1D, {{7, -0.5, 6.5}});

  // tpc histograms
  histos.add("Tracks/TPC/tpcNClsFindable", "number of findable TPC clusters;# findable clusters TPC", kTH1D, {{165, -0.5, 164.5}});
  histos.add("Tracks/TPC/tpcNClsFound", "number of found TPC clusters;# clusters TPC", kTH1D, {{165, -0.5, 164.5}});
  histos.add("Tracks/TPC/tpcNClsShared", "number of shared TPC clusters;# shared clusters TPC", kTH1D, {{165, -0.5, 164.5}});
  histos.add("Tracks/TPC/tpcNClsCrossedRows", "number of crossed TPC rows;# crossed rows TPC", kTH1D, {{165, -0.5, 164.5}});
  histos.add("Tracks/TPC/tpcFractionSharedCls", "fraction of shared TPC clusters;fraction shared clusters TPC", kTH1D, {{100, 0., 1.}});
  histos.add("Tracks/TPC/tpcCrossedRowsOverFindableCls", "crossed TPC rows over findable clusters;crossed rows / findable clusters TPC", kTH1D, {{120, 0.0, 1.2}});
  histos.add("Tracks/TPC/tpcChi2NCl", "chi2 per cluster in TPC;chi2 / cluster TPC", kTH1D, {{100, 0, 10}});
}

//**************************************************************************************************
/**
 * Check if track fulfils the configurable requirements.
 */
//**************************************************************************************************
template <bool IS_MC, typename T>
bool qaEventTrack::isSelectedTrack(const T& track)
{
  if (selectCharge && (selectCharge != track.sign())) {
    return false;
  }
  if constexpr (IS_MC) {
    auto particle = track.mcParticle();
    const bool isPrimary = particle.isPhysicalPrimary();
    if (selectPrim && !isPrimary) {
      return false;
    }
    if (selectSec && isPrimary) {
      return false;
    }
    if (selectPID && selectPID != std::abs(particle.pdgCode())) {
      return false;
    }
  }
  return true;
}

//**************************************************************************************************
/**
 * Fill reco level histograms.
 */
//**************************************************************************************************
template <bool IS_MC, typename C, typename T>
void qaEventTrack::processReco(const C& collision, const T& tracks)
{
  // fill reco collision related histograms
  histos.fill(HIST("Events/recoEff"), 1);
  if (selectGoodEvents && !(isRun3 ? collision.sel8() : collision.sel7())) { // currently only  sel8 is defined for run3
    return;
  }
  histos.fill(HIST("Events/recoEff"), 2);

  int nTracks = 0;
  for (const auto& track : tracks) {
    if (!isSelectedTrack<IS_MC>(track)) {
      continue;
    }
    ++nTracks;
  }

  histos.fill(HIST("Events/posX"), collision.posX());
  histos.fill(HIST("Events/posY"), collision.posY());
  histos.fill(HIST("Events/posZ"), collision.posZ());
  histos.fill(HIST("Events/posXY"), collision.posX(), collision.posY());

  histos.fill(HIST("Events/posXvsNContrib"), collision.posX(), collision.numContrib());
  histos.fill(HIST("Events/posYvsNContrib"), collision.posY(), collision.numContrib());
  histos.fill(HIST("Events/posZvsNContrib"), collision.posZ(), collision.numContrib());

  histos.fill(HIST("Events/nContrib"), collision.numContrib());
  histos.fill(HIST("Events/nContribVsMult"), collision.numContrib(), nTracks);
  histos.fill(HIST("Events/vertexChi2"), collision.chi2());

  histos.fill(HIST("Events/covXX"), collision.covXX());
  histos.fill(HIST("Events/covXY"), collision.covXY());
  histos.fill(HIST("Events/covXZ"), collision.covXZ());
  histos.fill(HIST("Events/covYY"), collision.covYY());
  histos.fill(HIST("Events/covYZ"), collision.covYZ());
  histos.fill(HIST("Events/covZZ"), collision.covZZ());

  histos.fill(HIST("Events/nTracks"), nTracks);

  // vertex resolution
  if constexpr (IS_MC) {
    const auto mcColl = collision.mcCollision();
    histos.fill(HIST("Events/resoX"), collision.posX() - mcColl.posX(), collision.numContrib());
    histos.fill(HIST("Events/resoY"), collision.posY() - mcColl.posY(), collision.numContrib());
    histos.fill(HIST("Events/resoZ"), collision.posZ() - mcColl.posZ(), collision.numContrib());
  }

  histos.fill(HIST("Tracks/recoEff"), 1, tracks.tableSize());
  histos.fill(HIST("Tracks/recoEff"), 2, tracks.size());

  // track related histograms
  for (const auto& track : tracks) {
    if (!isSelectedTrack<IS_MC>(track)) {
      continue;
    }
    // fill kinematic variables
    histos.fill(HIST("Tracks/Kine/pt"), track.pt());
    histos.fill(HIST("Tracks/Kine/eta"), track.eta());
    histos.fill(HIST("Tracks/Kine/phi"), track.phi());

    // fill track parameters
    histos.fill(HIST("Tracks/alpha"), track.alpha());
    histos.fill(HIST("Tracks/x"), track.x());
    histos.fill(HIST("Tracks/y"), track.y());
    histos.fill(HIST("Tracks/z"), track.z());
    histos.fill(HIST("Tracks/signed1Pt"), track.signed1Pt());
    histos.fill(HIST("Tracks/snp"), track.snp());
    histos.fill(HIST("Tracks/tgl"), track.tgl());
    for (unsigned int i = 0; i < 64; i++) {
      if (track.flags() & (1 << i)) {
        histos.fill(HIST("Tracks/flags"), i);
      }
    }
    histos.fill(HIST("Tracks/dcaXY"), track.dcaXY());
    histos.fill(HIST("Tracks/dcaZ"), track.dcaZ());
    histos.fill(HIST("Tracks/dcaXYvsPt"), track.dcaXY(), track.pt());
    histos.fill(HIST("Tracks/dcaZvsPt"), track.dcaZ(), track.pt());
    histos.fill(HIST("Tracks/length"), track.length());

    // fill ITS variables
    histos.fill(HIST("Tracks/ITS/itsNCls"), track.itsNCls());
    histos.fill(HIST("Tracks/ITS/itsChi2NCl"), track.itsChi2NCl());
    for (unsigned int i = 0; i < 7; i++) {
      if (track.itsClusterMap() & (1 << i)) {
        histos.fill(HIST("Tracks/ITS/itsHits"), i);
      }
    }

    // fill TPC variables
    histos.fill(HIST("Tracks/TPC/tpcNClsFindable"), track.tpcNClsFindable());
    histos.fill(HIST("Tracks/TPC/tpcNClsFound"), track.tpcNClsFound());
    histos.fill(HIST("Tracks/TPC/tpcNClsShared"), track.tpcNClsShared());
    histos.fill(HIST("Tracks/TPC/tpcNClsCrossedRows"), track.tpcNClsCrossedRows());
    histos.fill(HIST("Tracks/TPC/tpcCrossedRowsOverFindableCls"), track.tpcCrossedRowsOverFindableCls());
    histos.fill(HIST("Tracks/TPC/tpcFractionSharedCls"), track.tpcFractionSharedCls());
    histos.fill(HIST("Tracks/TPC/tpcChi2NCl"), track.tpcChi2NCl());

    if constexpr (IS_MC) {
      // resolution plots
      auto particle = track.mcParticle();
      histos.fill(HIST("Tracks/Kine/resoPt"), track.pt() - particle.pt(), track.pt());
      histos.fill(HIST("Tracks/Kine/resoEta"), track.eta() - particle.eta(), track.eta());
      histos.fill(HIST("Tracks/Kine/resoPhi"), track.phi() - particle.phi(), track.phi());
    }
  }
}
