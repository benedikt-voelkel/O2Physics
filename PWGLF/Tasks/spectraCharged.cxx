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

// task for charged particle pt spectra vs multiplicity analysis with 2d unfolding for run3+
// mimics https://github.com/alisw/AliPhysics/blob/master/PWGLF/SPECTRA/ChargedHadrons/MultDepSpec/AliMultDepSpecAnalysisTask.cxx

// run for data as: o2-analysis-timestamp | o2-analysis-event-selection | o2-analysis-trackextension | o2-analysis-trackselection | o2-analysis-lf-spectra-charged --aod-file AO2D_data.root
// run for mc as: o2-analysis-timestamp --isRun2MC | o2-analysis-event-selection --isMC | o2-analysis-trackextension | o2-analysis-trackselection | o2-analysis-lf-spectra-charged --aod-file AO2D_mc.root --isMC

#include "Framework/HistogramRegistry.h"
#include "ReconstructionDataFormats/Track.h"
#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/DataModel/Centrality.h"
#include "Common/Core/MC.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "TDatabasePDG.h"

using namespace o2;
using namespace o2::framework;

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
// Task declaration
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
struct chargedSpectra {

  HistogramRegistry histos;
  Service<TDatabasePDG> pdg;

  // TODO: can we derive this from config context resp from meta data in the file to avoid having to specify this option?
  Configurable<bool> isMC{"isMC", false, "option to flag mc"};
  Configurable<bool> isRun3{"isRun3", true, "Is Run3 dataset"}; // TODO: derive this from metadata once possible to get rid of the flag

  // task settings that can be steered via hyperloop
  Configurable<uint32_t> maxMultMeas{"measMult", 100, "max measured multiplicity."};
  Configurable<uint32_t> maxMultTrue{"measTrue", 100, "max true multiplicity."};

  // TODO: better use 2d configurables here
  Configurable<float> etaCut{"etaCut", 0.8f, "Eta cut."};
  Configurable<float> ptMinCut{"ptMinCut", 0.15f, "Pt min cut."};
  Configurable<float> ptMaxCut{"ptMaxCut", 10.f, "Pt max cut."};

  // helper struct to store transient properties
  struct varContainer {
    uint32_t multMeas{0};
    uint32_t multTrue{0};
    bool isAcceptedEvent{false};
    bool isAcceptedEventMC{false};
    bool isChargedPrimary{false};
  };
  varContainer vars;

  void init(InitContext const&);

  template <typename P>
  bool initParticle(const P& particle);

  template <typename T>
  bool initTrack(const T& track);

  template <typename C, typename T>
  void initEvent(const C& collision, const T& tracks);

  template <typename C, typename P>
  void initEventMC(const C& collision, const P& particles);

  template <bool IS_MC, typename C, typename T>
  void processMeas(const C& collision, const T& tracks);

  template <typename C, typename P>
  void processTrue(const C& collision, const P& particles);

  using CollisionTableData = soa::Join<aod::Collisions, aod::EvSels>;
  using TrackTableData = soa::Join<aod::Tracks, aod::TrackSelection>;
  void processData(CollisionTableData::iterator const& collision, TrackTableData const& tracks);
  PROCESS_SWITCH(chargedSpectra, processData, "process data", false);

  using CollisionTableMCTrue = aod::McCollisions;
  using CollisionTableMC = soa::SmallGroups<soa::Join<aod::McCollisionLabels, aod::Collisions, aod::EvSels>>;
  using TrackTableMC = soa::Join<aod::Tracks, aod::McTrackLabels, aod::TrackSelection>;
  using ParticleTableMC = aod::McParticles;
  void processMC(CollisionTableMCTrue::iterator const& mcCollision, CollisionTableMC const& collisions, TrackTableMC const& tracks, ParticleTableMC const& particles);
  PROCESS_SWITCH(chargedSpectra, processMC, "process mc", true);

  // TODO: - Milestone -  express most of the selections on events and tracks in a declarative way to improve performance
  /*
   add
    Filter xy;
    soa::Filtered<Table>

   For the collision and track tables (data and MC):
    - collision z pos < 10cm
    - trigger condition + event selection
    - track selection + is in kine range

   For the MC tables we need to keep everything that EITHER fulfils the conditions in data OR in MC to get correct efficiencies and contamination!
  */
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{adaptAnalysisTask<chargedSpectra>(cfgc)};
}

//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------
// Task implementation
//--------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------

//**************************************************************************************************
/**
 * Initialise the task and add histograms.
 */
//**************************************************************************************************
void chargedSpectra::init(InitContext const&)
{
  std::vector<double> ptBinEdges = {0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.55, 0.6, 0.65, 0.7, 0.75,
                                    0.8, 0.85, 0.9, 0.95, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7, 1.8, 1.9,
                                    2.0, 2.2, 2.4, 2.6, 2.8, 3.0, 3.2, 3.4, 3.6, 3.8, 4.0, 4.5, 5.0, 5.5,
                                    6.0, 6.5, 7.0, 8.0, 9.0, 10.0};

  const AxisSpec ptMeasAxis{ptBinEdges, "#it{p}^{ meas}_{T} (GeV/#it{c})", "pt_meas"};

  const int nBinsMultMeas = maxMultMeas + 1;
  const AxisSpec multMeasAxis = {nBinsMultMeas, -0.5, nBinsMultMeas - 0.5, "#it{N}^{ meas}_{ch}", "mult_meas"};

  histos.add("multDist_evt_meas", "", kTH1D, {multMeasAxis});               // measured event distribution (contains contamination from events not in specified class or with wrong vertex position)
  histos.add("multPtSpec_trk_meas", "", kTH2D, {multMeasAxis, ptMeasAxis}); // measured tracks (contains contamination from secondary particles, particles smeared into acceptance and tracks originating from background events as defined above )

  if (isMC) {

    const AxisSpec ptTrueAxis{ptBinEdges, "#it{p}_{T} (GeV/c)", "pt_true"};

    const int nBinsMultTrue = maxMultTrue + 1;
    const AxisSpec multTrueAxis = {nBinsMultTrue, -0.5, nBinsMultTrue - 0.5, "#it{N}_{ch}", "mult_true"};

    histos.add("collision_ambiguity", "", kTH1D, {{6, -0.5, 5.5, "reco collisions per true collision"}}); // log the number of collisions that were reconstructed for a MC collision

    histos.add("multDist_evt_gen", "", kTH1D, {multTrueAxis});      // generated event distribution  (from events within specified class and with proper vertex position)
    histos.add("multDist_evt_gen_trig", "", kTH1D, {multTrueAxis}); // generated event distribution (from events within specified class and with proper vertex position) that in addition fulfils the trigger condition [to disentangle trigger eff from reco eff ]

    histos.add("multCorrel_evt", "", kTH2D, {multMeasAxis, multTrueAxis});  // multiplicity correlation of measured events (excluding background events)
    histos.add("multCorrel_prim", "", kTH2D, {multMeasAxis, multTrueAxis}); // multiplicity correlation of measured primary charged particles (excluding particles from background events)
    histos.add("ptCorrel_prim", "", kTH2D, {ptMeasAxis, ptTrueAxis});       // pT correlation of measured primary charged particles  (excluding particles from background events)

    histos.add("multPtSpec_prim_gen", "", kTH2D, {multTrueAxis, ptTrueAxis});         // generated primary charged particles as function of true properties (from events within specified class and with proper vertex position)
    histos.add("multPtSpec_prim_gen_evtloss", "", kTH2D, {multTrueAxis, ptTrueAxis}); // generated primary charged particles of events that did not pass the event selection as function of multiplicity and pt
    histos.add("multPtSpec_prim_gen_notrig", "", kTH2D, {multTrueAxis, ptTrueAxis});  // generated primary charged particles of events that did not fulfil physics selection and trigger condition as function of multiplicity and pt
    histos.add("multPtSpec_prim_meas", "", kTH2D, {multTrueAxis, ptTrueAxis});        // measured primary charged particles as function of true properties (no contamination from background events)

    histos.add("multPtSpec_trk_prim_meas", "", kTH2D, {multMeasAxis, ptMeasAxis});    // tracks from measured primaries (no contamination from secondaries, particles smeared into acceptance or background events)
    histos.add("multPtSpec_trk_sec_meas", "", kTH2D, {multMeasAxis, ptMeasAxis});     // tracks from measured secondaries (no contamination from particles smeared into acceptance or background events)  [for QA to disentangle secondaries from other contamination]
    histos.add("multPtSpec_trk_meas_evtcont", "", kTH2D, {multMeasAxis, ptMeasAxis}); // tracks from events that are measured, but do not belong to the desired class of events
  }
}

//**************************************************************************************************
/**
 * Entrypoint to processes data.
 */
//**************************************************************************************************
void chargedSpectra::processData(CollisionTableData::iterator const& collision, TrackTableData const& tracks)
{
  initEvent(collision, tracks);
  processMeas<false>(collision, tracks);
}

//**************************************************************************************************
/**
 * Entrypoint to processes mc.
 */
//**************************************************************************************************
void chargedSpectra::processMC(CollisionTableMCTrue::iterator const& mcCollision, CollisionTableMC const& collisions, TrackTableMC const& tracks, ParticleTableMC const& particles)
{
  histos.fill(HIST("collision_ambiguity"), collisions.size());

  // TODO: process only most probable collision (run3)
  if (collisions.size() > 1) {
    // FIXME: for now skip all ambiguously reconstructed collisions as we do not know what to do with them
    return;
  }
  // MEMO: this ambiguity of the reconstructed collisions raises several questions:
  // 1st: how to select most probable collision?
  // 2nd: how to avoid double or triple counting of an actual collision in data (or how to treat this as additional contamination of the measurement based on MC info)
  // 3rd: how does this pollute the event reconstruction efficiency

  initEventMC(mcCollision, particles);
  processTrue(mcCollision, particles);

  for (auto& collision : collisions) {
    auto curTracks = tracks.sliceBy(aod::track::collisionId, collision.globalIndex());
    initEvent(collision, curTracks);
    processMeas<true>(collision, curTracks);
    break; // for now look only at one collision...
  }
}

//**************************************************************************************************
/**
 * Check if particle is good.
 */
//**************************************************************************************************
template <typename P>
bool chargedSpectra::initParticle(const P& particle)
{
  vars.isChargedPrimary = false;
  auto pdgParticle = pdg->GetParticle(particle.pdgCode());
  if (!pdgParticle || pdgParticle->Charge() == 0.) {
    return false;
  }
  vars.isChargedPrimary = particle.isPhysicalPrimary();
  if (std::abs(particle.eta()) >= etaCut) {
    return false;
  }
  if (particle.pt() <= ptMinCut || particle.pt() >= ptMaxCut) {
    return false;
  }
  return true;
}

//**************************************************************************************************
/**
 * Check if track is good.
 */
//**************************************************************************************************
template <typename T>
bool chargedSpectra::initTrack(const T& track)
{
  if (std::abs(track.eta()) >= etaCut) {
    return false;
  }
  if (track.pt() <= ptMinCut || track.pt() >= ptMaxCut) {
    return false;
  }
  // TODO: with Filters we could skip this in data, but not in MC (maybe add IS_MC template paramter so we can skip it in data via if constexpr)
  if (!track.isGlobalTrack() /*track.trackType() !=  o2::aod::track::Track*/) {
    return false;
    // MEMO: current version of the track selection cuts too harshly (to be studied why) and therefore many events have multMeas==0
    // as temporary workaround to look at unselected tracks use the commented out condition instead
  }

  return true;
}

//**************************************************************************************************
/**
 * Check if event is good.
 */
//**************************************************************************************************
template <typename C, typename T>
void chargedSpectra::initEvent(const C& collision, const T& tracks)
{
  vars.multMeas = 0;
  for (auto& track : tracks) {
    if (initTrack(track)) {
      ++vars.multMeas;
    }
  }

  vars.isAcceptedEvent = false;
  if ((collision.posZ() < 10.f) && isRun3 ? collision.sel8() : (collision.alias()[kINT7] && collision.sel7())) {
    vars.isAcceptedEvent = true;
  }
}

//**************************************************************************************************
/**
 * Check if MC event is good.
 */
//**************************************************************************************************
template <typename C, typename P>
void chargedSpectra::initEventMC(const C& collision, const P& particles)
{
  vars.multTrue = 0;
  for (auto& particle : particles) {
    if (!initParticle(particle) || !vars.isChargedPrimary) {
      continue;
    }
    ++vars.multTrue;
  }
  // TODO: also determine event class and check if true z vtx positin is good
  vars.isAcceptedEventMC = ((collision.posZ() < 10.f) && (vars.multTrue > 0));
}

//**************************************************************************************************
/**
 * Function to processes MC truth info. Assumes initEventMC havs been called previously.
 */
//**************************************************************************************************
template <typename C, typename P>
void chargedSpectra::processTrue(const C& collision, const P& particles)
{
  if (vars.isAcceptedEventMC) {
    histos.fill(HIST("multDist_evt_gen"), vars.multTrue);
  }

  for (auto& particle : particles) {
    if (initParticle(particle) && vars.isChargedPrimary) {
      if (vars.isAcceptedEventMC) {
        // TODO: event class selection, etc.
        histos.fill(HIST("multPtSpec_prim_gen"), vars.multTrue, particle.pt());
        if (!vars.isAcceptedEvent) {
          histos.fill(HIST("multPtSpec_prim_gen_evtloss"), vars.multTrue, particle.pt());
        }
      }
    }
  }
}

//**************************************************************************************************
/**
 * Function to process reconstructed data and MC. Assumes initEvent (and initEventMC in case of MC) have been called previously.
 */
//**************************************************************************************************
template <bool IS_MC, typename C, typename T>
void chargedSpectra::processMeas(const C& collision, const T& tracks)
{
  if (!vars.isAcceptedEvent) {
    return;
  }

  histos.fill(HIST("multDist_evt_meas"), vars.multMeas);

  if constexpr (IS_MC) {
    if (vars.isAcceptedEventMC) {
      histos.fill(HIST("multCorrel_evt"), vars.multMeas, vars.multTrue);
    }
  }

  for (auto& track : tracks) {

    if (!initTrack(track)) {
      continue;
    }

    histos.fill(HIST("multPtSpec_trk_meas"), vars.multMeas, track.pt());

    if constexpr (IS_MC) {

      const auto& particle = track.mcParticle();

      if (!vars.isAcceptedEventMC) {
        histos.fill(HIST("multPtSpec_trk_meas_evtcont"), vars.multMeas, track.pt());
        continue;
      }

      if (initParticle(particle)) {

        if (!vars.isChargedPrimary) {
          histos.fill(HIST("multPtSpec_trk_sec_meas"), vars.multMeas, track.pt());
        } else {
          histos.fill(HIST("multCorrel_prim"), vars.multMeas, vars.multTrue);
          histos.fill(HIST("ptCorrel_prim"), track.pt(), particle.pt());
          histos.fill(HIST("multPtSpec_prim_meas"), vars.multTrue, particle.pt());
          histos.fill(HIST("multPtSpec_trk_prim_meas"), vars.multMeas, track.pt());
        }
      }
    }
  }
}
