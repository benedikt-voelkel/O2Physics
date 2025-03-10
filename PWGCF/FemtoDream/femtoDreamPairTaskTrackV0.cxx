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

/// \file femtoDreamPairTaskTrackTrack.cxx
/// \brief Tasks that reads the track tables used for the pairing and builds pairs of two tracks
/// \author Andi Mathis, TU München, andreas.mathis@ph.tum.de

#include "PWGCF/DataModel/FemtoDerived.h"
#include "FemtoDreamParticleHisto.h"
#include "FemtoDreamEventHisto.h"
#include "FemtoDreamPairCleaner.h"
#include "FemtoDreamContainer.h"
#include "FemtoDreamDetaDphiStar.h"
#include <CCDB/BasicCCDBManager.h>
#include "DataFormatsParameters/GRPObject.h"
#include "Framework/AnalysisTask.h"
#include "Framework/runDataProcessing.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/ASoAHelpers.h"

using namespace o2;
using namespace o2::analysis::femtoDream;
using namespace o2::framework;
using namespace o2::framework::expressions;

namespace
{
static constexpr int nCuts = 4;
static const std::vector<std::string> cutNames{"MaxPt", "PIDthr", "nSigmaTPC", "nSigmaTPCTOF"};
static const float cutsArray[nCuts] = {4.05f, 0.75f, 3.f, 3.f};

static constexpr int nNsigma = 3;
static constexpr float kNsigma[nNsigma] = {3.5f, 3.f, 2.5f};

enum kPIDselection {
  k3d5sigma = 0,
  k3sigma = 1,
  k2d5sigma = 2
};

enum kDetector {
  kTPC = 0,
  kTPCTOF = 1,
  kNdetectors = 2
};
} // namespace

struct femtoDreamPairTaskTrackV0 {

  /// Particle 1 (track)
  Configurable<int> ConfPDGCodePartOne{"ConfPDGCodePartOne", 2212, "Particle 1 - PDG code"};
  Configurable<uint32_t> ConfCutPartOne{"ConfCutPartOne", 5542986, "Particle 1 - Selection bit from cutCulator"};
  Configurable<std::vector<int>> ConfPIDPartOne{"ConfPIDPartOne", std::vector<int>{2}, "Particle 1 - Read from cutCulator"}; // we also need the possibility to specify whether the bit is true/false ->std>>vector<std::pair<int, int>>int>>
  Configurable<LabeledArray<float>> cfgCutArray{"cfgCutArray", {cutsArray, nCuts, cutNames}, "Particle selections"};
  Configurable<int> cfgNspecies{"ccfgNspecies", 4, "Number of particle spieces with PID info"};

  /// Partition for particle 1
  Partition<aod::FemtoDreamParticles> partsOne = (aod::femtodreamparticle::partType == uint8_t(aod::femtodreamparticle::ParticleType::kTrack)) &&
                                                 (aod::femtodreamparticle::pt < cfgCutArray->get("MaxPt")) &&
                                                 ((aod::femtodreamparticle::cut & ConfCutPartOne) == ConfCutPartOne);

  /// Histogramming for particle 1
  FemtoDreamParticleHisto<aod::femtodreamparticle::ParticleType::kTrack, 1> trackHistoPartOne;

  /// Particle 2 (V0)
  Configurable<int> ConfPDGCodePartTwo{"ConfPDGCodePartTwo", 3122, "Particle 1 - PDG code"};
  Configurable<uint32_t> ConfCutPartTwo{"ConfCutPartTwo", 338, "Particle 2 - Selection bit"};

  /// Partition for particle 2
  Partition<aod::FemtoDreamParticles> partsTwo = (aod::femtodreamparticle::partType == uint8_t(aod::femtodreamparticle::ParticleType::kV0)) &&
                                                 ((aod::femtodreamparticle::cut & ConfCutPartTwo) == ConfCutPartTwo);

  /// Histogramming for particle 2
  FemtoDreamParticleHisto<aod::femtodreamparticle::ParticleType::kV0, 2> trackHistoPartTwo;

  /// Histogramming for Event
  FemtoDreamEventHisto eventHisto;

  /// The configurables need to be passed to an std::vector
  std::vector<int> vPIDPartOne;

  /// Correlation part
  ConfigurableAxis CfgMultBins{"CfgMultBins", {VARIABLE_WIDTH, 0.0f, 20.0f, 40.0f, 60.0f, 80.0f, 100.0f, 200.0f, 99999.f}, "Mixing bins - multiplicity"};
  ConfigurableAxis CfgkstarBins{"CfgkstarBins", {1500, 0., 6.}, "binning kstar"};
  ConfigurableAxis CfgkTBins{"CfgkTBins", {150, 0., 9.}, "binning kT"};
  ConfigurableAxis CfgmTBins{"CfgmTBins", {225, 0., 7.5}, "binning mT"};
  Configurable<int> ConfNEventsMix{"ConfNEventsMix", 5, "Number of events for mixing"};
  Configurable<bool> ConfIsCPR{"ConfIsCPR", true, "Close Pair Rejection"};
  Configurable<float> ConfBField{"ConfBField", +0.5, "Magnetic Field"};

  FemtoDreamContainer<femtoDreamContainer::EventType::same, femtoDreamContainer::Observable::kstar> sameEventCont;
  FemtoDreamContainer<femtoDreamContainer::EventType::mixed, femtoDreamContainer::Observable::kstar> mixedEventCont;
  FemtoDreamPairCleaner<aod::femtodreamparticle::ParticleType::kTrack, aod::femtodreamparticle::ParticleType::kV0> pairCleaner;
  FemtoDreamDetaDphiStar<aod::femtodreamparticle::ParticleType::kTrack, aod::femtodreamparticle::ParticleType::kV0> pairCloseRejection;
  /// Histogram output
  HistogramRegistry qaRegistry{"TrackQA", {}, OutputObjHandlingPolicy::AnalysisObject};
  HistogramRegistry resultRegistry{"Correlations", {}, OutputObjHandlingPolicy::AnalysisObject};

  Service<o2::ccdb::BasicCCDBManager> ccdb; ///Accessing the CCDB

  void init(InitContext&)
  {
    eventHisto.init(&qaRegistry);
    trackHistoPartOne.init(&qaRegistry);
    trackHistoPartTwo.init(&qaRegistry);

    sameEventCont.init(&resultRegistry, CfgkstarBins, CfgMultBins, CfgkTBins, CfgmTBins);
    sameEventCont.setPDGCodes(ConfPDGCodePartOne, ConfPDGCodePartTwo);
    mixedEventCont.init(&resultRegistry, CfgkstarBins, CfgMultBins, CfgkTBins, CfgmTBins);
    mixedEventCont.setPDGCodes(ConfPDGCodePartOne, ConfPDGCodePartTwo);
    pairCleaner.init(&qaRegistry);
    if (ConfIsCPR) {
      pairCloseRejection.init(&resultRegistry, &qaRegistry, 0.01, 0.01, false); /// \todo add config for Δη and ΔΦ cut values
    }

    vPIDPartOne = ConfPIDPartOne;

    /// Initializing CCDB
    ccdb->setURL("http://alice-ccdb.cern.ch");
    ccdb->setCaching(true);
    ccdb->setLocalObjectValidityChecking();

    long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    ccdb->setCreatedNotAfter(now);
  }

  /// Function to retrieve the nominal mgnetic field in kG (0.1T) and convert it directly to T
  float getMagneticFieldTesla(uint64_t timestamp)
  {
    // TODO done only once (and not per run). Will be replaced by CCDBConfigurable
    static o2::parameters::GRPObject* grpo = nullptr;
    if (grpo == nullptr) {
      grpo = ccdb->getForTimeStamp<o2::parameters::GRPObject>("GLO/GRP/GRP", timestamp);
      if (grpo == nullptr) {
        LOGF(fatal, "GRP object not found for timestamp %llu", timestamp);
        return 0;
      }
      LOGF(info, "Retrieved GRP for timestamp %llu with magnetic field of %d kG", timestamp, grpo->getNominalL3Field());
    }
    float output = 0.1 * (grpo->getNominalL3Field());
    return output;
  }

  /// internal function that returns the kPIDselection element corresponding to a specifica n-sigma value
  /// \param number of sigmas for PIF
  /// \return kPIDselection corresponing to n-sigma
  kPIDselection getPIDselection(float nSigma)
  {
    for (int i = 0; i < nNsigma; i++) {
      if (abs(nSigma - kNsigma[i]) < 1e-3) {
        return static_cast<kPIDselection>(i);
      }
    }
    LOG(info) << "Invalid value of nSigma: " << nSigma << ". Standard 3 sigma returned." << std::endl;
    return kPIDselection::k3sigma;
  }

  /// function that checks whether the PID selection specified in the vectors is fulfilled
  /// \param pidcut Bit-wise container for the PID
  /// \param vSpecies Vector with the different selections
  /// \return Whether the PID selection specified in the vectors is fulfilled
  bool isPIDSelected(aod::femtodreamparticle::cutContainerType const& pidcut, std::vector<int> const& vSpecies, float nSigma, kDetector iDet = kDetector::kTPC)
  {
    bool pidSelection = true;
    kPIDselection kNsigma = getPIDselection(nSigma);
    for (auto iSpecies : vSpecies) {
      //\todo we also need the possibility to specify whether the bit is true/false ->std>>vector<std::pair<int, int>>
      // if (!((pidcut >> it.first) & it.second)) {
      int bit_to_check = cfgNspecies * kDetector::kNdetectors * kNsigma + iSpecies * kDetector::kNdetectors + iDet;
      if (!(pidcut & (1UL << bit_to_check))) {
        pidSelection = false;
      }
    }
    return pidSelection;
  };

  /// function that checks whether the PID selection specified in the vectors is fulfilled, depending on the momentum TPC or TPC+TOF PID is conducted
  /// \param pidcut Bit-wise container for the PID
  /// \param mom Momentum of the track
  /// \param pidThresh Momentum threshold that separates between TPC and TPC+TOF PID
  /// \param vecTPC Vector with the different selections for the TPC PID
  /// \param vecComb Vector with the different selections for the TPC+TOF PID
  /// \return Whether the PID selection is fulfilled
  bool isFullPIDSelected(aod::femtodreamparticle::cutContainerType const& pidCut, float const& momentum, float const& pidThresh, std::vector<int> const& vSpecies, float nSigmaTPC = 3.5, float nSigmaTPCTOF = 3.5)
  {
    bool pidSelection = true;
    if (momentum < pidThresh) {
      /// TPC PID only
      pidSelection = isPIDSelected(pidCut, vSpecies, nSigmaTPC, kDetector::kTPC);
    } else {
      /// TPC + TOF PID
      pidSelection = isPIDSelected(pidCut, vSpecies, nSigmaTPCTOF, kDetector::kTPCTOF);
    }
    return pidSelection;
  };

  /// This function processes the same event and takes care of all the histogramming
  /// \todo the trivial loops over the tracks should be factored out since they will be common to all combinations of T-T, T-V0, V0-V0, ...
  void processSameEvent(o2::aod::FemtoDreamCollision& col,
                        o2::aod::FemtoDreamParticles& parts)
  {
    const int multCol = col.multV0M();
    eventHisto.fillQA(col);
    /// Histogramming same event
    for (auto& part : partsOne) {
      if (!isFullPIDSelected(part.pidcut(), part.p(), cfgCutArray->get("PIDthr"), vPIDPartOne, cfgCutArray->get("nSigmaTPC"), cfgCutArray->get("nSigmaTPCTOF"))) {
        continue;
      }
      trackHistoPartOne.fillQA(part);
    }
    for (auto& part : partsTwo) {
      trackHistoPartTwo.fillQA(part);
    }
    /// Now build the combinations
    for (auto& [p1, p2] : combinations(partsOne, partsTwo)) {
      if (!isFullPIDSelected(p1.pidcut(), p1.p(), cfgCutArray->get("PIDthr"), vPIDPartOne, cfgCutArray->get("nSigmaTPC"), cfgCutArray->get("nSigmaTPCTOF"))) {
        continue;
      }

      auto tmstamp = col.timestamp();
      if (ConfIsCPR) {
        if (pairCloseRejection.isClosePair(p1, p2, parts, getMagneticFieldTesla(tmstamp))) {
          continue;
        }
      }

      // track cleaning
      if (!pairCleaner.isCleanPair(p1, p2, parts)) {
        continue;
      }
      sameEventCont.setPair(p1, p2, multCol);
    }
  }

  PROCESS_SWITCH(femtoDreamPairTaskTrackV0, processSameEvent, "Enable processing same event", true);

  /// This function processes the mixed event
  /// \todo the trivial loops over the collisions and tracks should be factored out since they will be common to all combinations of T-T, T-V0, V0-V0, ...
  void processMixedEvent(o2::aod::FemtoDreamCollisions& cols,
                         o2::aod::Hashes& hashes,
                         o2::aod::FemtoDreamParticles& parts)
  {
    cols.bindExternalIndices(&parts);
    auto particlesTuple = std::make_tuple(parts);
    GroupSlicer slicer(cols, particlesTuple);

    for (auto& [collision1, collision2] : soa::selfCombinations("fBin", ConfNEventsMix, -1, soa::join(hashes, cols), soa::join(hashes, cols))) {
      auto it1 = slicer.begin();
      auto it2 = slicer.begin();
      for (auto& slice : slicer) {
        if (slice.groupingElement().index() == collision1.index()) {
          it1 = slice;
          break;
        }
      }
      for (auto& slice : slicer) {
        if (slice.groupingElement().index() == collision2.index()) {
          it2 = slice;
          break;
        }
      }

      auto particles1 = std::get<aod::FemtoDreamParticles>(it1.associatedTables());
      particles1.bindExternalIndices(&cols);
      auto particles2 = std::get<aod::FemtoDreamParticles>(it2.associatedTables());
      particles2.bindExternalIndices(&cols);

      partsOne.bindTable(particles1);
      partsTwo.bindTable(particles2);

      /// \todo before mixing we should check whether both collisions contain a pair of particles!
      /// could work like that, but only if PID is contained within the partitioning!
      // auto particlesEvent1 = std::get<aod::FemtoDreamParticles>(it1.associatedTables());
      // particlesEvent1.bindExternalIndices(&cols);
      // auto particlesEvent2 = std::get<aod::FemtoDreamParticles>(it2.associatedTables());
      // particlesEvent2.bindExternalIndices(&cols);
      /// for the x-check
      // partsOne.bindTable(particlesEvent2);
      // auto nPart1Evt2 = partsOne.size();
      // partsTwo.bindTable(particlesEvent1);
      // auto nPart2Evt1 = partsTwo.size();
      /// for actual event mixing
      // partsOne.bindTable(particlesEvent1);
      // partsTwo.bindTable(particlesEvent2);
      // if (partsOne.size() == 0 || nPart2Evt1 == 0 || nPart1Evt2 == 0 || partsTwo.size() == 0 ) continue;

      for (auto& [p1, p2] : combinations(partsOne, partsTwo)) {
        if (!isFullPIDSelected(p1.pidcut(), p1.p(), cfgCutArray->get("PIDthr"), vPIDPartOne, cfgCutArray->get("nSigmaTPC"), cfgCutArray->get("nSigmaTPCTOF"))) {
          continue;
        }
        if (ConfIsCPR) {
          if (pairCloseRejection.isClosePair(p1, p2, parts, getMagneticFieldTesla(collision1.timestamp()))) {
            continue;
          }
        }
        mixedEventCont.setPair(p1, p2, collision1.multV0M()); // < \todo dirty trick, the multiplicity will be of course within the bin width used for the hashes
      }
    }
  }

  PROCESS_SWITCH(femtoDreamPairTaskTrackV0, processMixedEvent, "Enable processing mixed events", true);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  WorkflowSpec workflow{
    adaptAnalysisTask<femtoDreamPairTaskTrackV0>(cfgc),
  };
  return workflow;
}
