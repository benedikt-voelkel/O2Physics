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

/// \file HFCandidateCreatorX.cxx
/// \brief Reconstruction of X(3872) candidates
/// \note Adapted from HFCandidateCreator3Prong
///
/// \author Rik Spijkers <r.spijkers@students.uu.nl>, Utrecht University
/// \author Luca Micheletti <luca.micheletti@to.infn.it>, INFN

#include "Framework/AnalysisTask.h"
#include "DetectorsVertexing/DCAFitterN.h"
#include "PWGHF/DataModel/HFSecondaryVertex.h"
#include "Common/Core/trackUtilities.h"
#include "ReconstructionDataFormats/DCA.h"
#include "ReconstructionDataFormats/V0.h"
#include "PWGHF/DataModel/HFCandidateSelectionTables.h"

using namespace o2;
using namespace o2::aod;
using namespace o2::framework;
using namespace o2::aod::hf_cand;
using namespace o2::aod::hf_cand_prong2;
using namespace o2::aod::hf_cand_x;
using namespace o2::framework::expressions;

void customize(std::vector<o2::framework::ConfigParamSpec>& workflowOptions)
{
  ConfigParamSpec optionDoMC{"doMC", VariantType::Bool, true, {"Perform MC matching."}};
  workflowOptions.push_back(optionDoMC);
}

#include "Framework/runDataProcessing.h"

/// Reconstruction of X candidates
struct HFCandidateCreatorX {
  Produces<aod::HfCandXBase> rowCandidateBase;

  Configurable<double> magneticField{"magneticField", 5., "magnetic field"};
  Configurable<bool> b_propdca{"b_propdca", true, "create tracks version propagated to PCA"};
  Configurable<double> d_maxr{"d_maxr", 200., "reject PCA's above this radius"};
  Configurable<double> d_maxdzini{"d_maxdzini", 4., "reject (if>0) PCA candidate if tracks DZ exceeds threshold"};
  Configurable<double> d_minparamchange{"d_minparamchange", 1.e-3, "stop iterations if largest change of any X is smaller than this"};
  Configurable<double> d_minrelchi2change{"d_minrelchi2change", 0.9, "stop iterations is chi2/chi2old > this"};
  Configurable<double> ptPionMin{"ptPionMin", 0.15, "minimum pion pT threshold (GeV/c)"};
  Configurable<bool> b_dovalplots{"b_dovalplots", true, "do validation plots"};

  OutputObj<TH1F> hMassJpsiToEE{TH1F("hMassJpsiToEE", "J/#psi candidates;inv. mass (e^{#plus} e^{#minus}) (GeV/#it{c}^{2});entries", 500, 0., 5.)};
  OutputObj<TH1F> hMassJpsiToMuMu{TH1F("hMassJpsiToMuMu", "J/#psi candidates;inv. mass (#mu^{#plus} #mu^{#minus}) (GeV/#it{c}^{2});entries", 500, 0., 5.)};
  OutputObj<TH1F> hPtJpsi{TH1F("hPtJpsi", "J/#psi candidates;candidate #it{p}_{T} (GeV/#it{c});entries", 100, 0., 10.)};
  OutputObj<TH1F> hPtPion{TH1F("hPtPion", "#pi candidates;candidate #it{p}_{T} (GeV/#it{c});entries", 100, 0., 10.)};
  OutputObj<TH1F> hCPAJpsi{TH1F("hCPAJpsi", "J/#psi candidates;cosine of pointing angle;entries", 110, -1.1, 1.1)};
  OutputObj<TH1F> hMassXToJpsiToEEPiPi{TH1F("hMassXToJpsiToEEPiPi", "3-prong candidates;inv. mass (J/#psi (#rightarrow e+ e-) #pi+ #pi-) (GeV/#it{c}^{2});entries", 500, 0., 5.)};
  OutputObj<TH1F> hMassXToJpsiToMuMuPiPi{TH1F("hMassXToJpsiToMuMuPiPi", "3-prong candidates;inv. mass (J/#psi (#rightarrow #mu+ #mu-) #pi+ #pi-) (GeV/#it{c}^{2});entries", 500, 0., 5.)};
  OutputObj<TH1F> hCovPVXX{TH1F("hCovPVXX", "3-prong candidates;XX element of cov. matrix of prim. vtx position (cm^{2});entries", 100, 0., 1.e-4)};
  OutputObj<TH1F> hCovSVXX{TH1F("hCovSVXX", "3-prong candidates;XX element of cov. matrix of sec. vtx position (cm^{2});entries", 100, 0., 0.2)};

  double massPi = RecoDecay::getMassPDG(kPiPlus);
  double massJpsi = RecoDecay::getMassPDG(443);
  double massJpsiPiPi;

  Configurable<int> d_selectionFlagJpsi{"d_selectionFlagJpsi", 1, "Selection Flag for Jpsi"};
  Configurable<double> cutYCandMax{"cutYCandMax", -1., "max. cand. rapidity"};
  Filter filterSelectCandidates = (aod::hf_selcandidate_jpsi::isSelJpsiToEE >= d_selectionFlagJpsi || aod::hf_selcandidate_jpsi::isSelJpsiToMuMu >= d_selectionFlagJpsi);

  void process(aod::Collision const& collision,
               soa::Filtered<soa::Join<
                 aod::HfCandProng2,
                 aod::HFSelJpsiCandidate>> const& jpsiCands,
               aod::BigTracks const& tracks)
  {
    // 2-prong vertex fitter (to rebuild Jpsi vertex)
    o2::vertexing::DCAFitterN<2> df2;
    df2.setBz(magneticField);
    df2.setPropagateToPCA(b_propdca);
    df2.setMaxR(d_maxr);
    df2.setMaxDZIni(d_maxdzini);
    df2.setMinParamChange(d_minparamchange);
    df2.setMinRelChi2Change(d_minrelchi2change);
    df2.setUseAbsDCA(true);

    // 3-prong vertex fitter
    o2::vertexing::DCAFitterN<3> df3;
    df3.setBz(magneticField);
    df3.setPropagateToPCA(b_propdca);
    df3.setMaxR(d_maxr);
    df3.setMaxDZIni(d_maxdzini);
    df3.setMinParamChange(d_minparamchange);
    df3.setMinRelChi2Change(d_minrelchi2change);
    df3.setUseAbsDCA(true);

    // loop over Jpsi candidates
    for (auto& jpsiCand : jpsiCands) {
      if (!(jpsiCand.hfflag() & 1 << hf_cand_prong2::DecayType::JpsiToEE) && !(jpsiCand.hfflag() & 1 << hf_cand_prong2::DecayType::JpsiToMuMu)) {
        continue;
      }
      if (cutYCandMax >= 0. && std::abs(YJpsi(jpsiCand)) > cutYCandMax) {
        continue;
      }
      if (jpsiCand.isSelJpsiToEE() > 0) {
        hMassJpsiToEE->Fill(InvMassJpsiToEE(jpsiCand));
      }
      if (jpsiCand.isSelJpsiToMuMu() > 0) {
        hMassJpsiToMuMu->Fill(InvMassJpsiToMuMu(jpsiCand));
      }
      hPtJpsi->Fill(jpsiCand.pt());
      hCPAJpsi->Fill(jpsiCand.cpa());
      // create Jpsi track to pass to DCA fitter; use cand table + rebuild vertex
      const std::array<float, 3> vertexJpsi = {jpsiCand.xSecondaryVertex(), jpsiCand.ySecondaryVertex(), jpsiCand.zSecondaryVertex()};
      array<float, 3> pvecJpsi = {jpsiCand.px(), jpsiCand.py(), jpsiCand.pz()};
      auto prong0 = jpsiCand.index0_as<aod::BigTracks>();
      auto prong1 = jpsiCand.index1_as<aod::BigTracks>();
      auto prong0TrackParCov = getTrackParCov(prong0);
      auto prong1TrackParCov = getTrackParCov(prong1);

      if (df2.process(prong0TrackParCov, prong1TrackParCov) == 0) {
        continue;
      }

      // propogate prong tracks to Jpsi vertex
      prong0TrackParCov.propagateTo(jpsiCand.xSecondaryVertex(), magneticField);
      prong1TrackParCov.propagateTo(jpsiCand.xSecondaryVertex(), magneticField);
      const std::array<float, 6> covJpsi = df2.calcPCACovMatrixFlat();
      // define the Jpsi track
      auto trackJpsi = o2::dataformats::V0(vertexJpsi, pvecJpsi, covJpsi, prong0TrackParCov, prong1TrackParCov, {0, 0}, {0, 0}); //FIXME: also needs covxyz???

      // used to check that prongs used for Jpsi and X reco are not the same prongs
      int index0Jpsi = jpsiCand.index0Id();
      int index1Jpsi = jpsiCand.index1Id();

      // loop over pi+ candidates
      for (auto& trackPos : tracks) {
        if (trackPos.sign() < 0) { // select only positive tracks - use partitions?
          continue;
        }
        if (trackPos.globalIndex() == index0Jpsi) {
          continue;
        }
        if (trackPos.pt() < ptPionMin) {
          continue;
        }
        hPtPion->Fill(trackPos.pt());

        // loop over pi- candidates
        for (auto& trackNeg : tracks) {
          if (trackNeg.sign() > 0) { // select only negative tracks - use partitions?
            continue;
          }
          if (trackNeg.globalIndex() == index1Jpsi) {
            continue;
          }
          if (trackNeg.pt() < ptPionMin) {
            continue;
          }
          hPtPion->Fill(trackNeg.pt());

          auto trackParVarPos = getTrackParCov(trackPos);
          auto trackParVarNeg = getTrackParCov(trackNeg);
          array<float, 3> pvecPos;
          array<float, 3> pvecNeg;

          // reconstruct the 3-prong X vertex
          if (df3.process(trackJpsi, trackParVarPos, trackParVarNeg) == 0) {
            continue;
          }

          // calculate relevant properties
          const auto& XsecondaryVertex = df3.getPCACandidate();
          auto chi2PCA = df3.getChi2AtPCACandidate();
          auto covMatrixPCA = df3.calcPCACovMatrixFlat();
          hCovSVXX->Fill(covMatrixPCA[0]); // FIXME: Calculation of errorDecayLength(XY) gives wrong values without this line.

          df3.propagateTracksToVertex();          // propagate the pions and Jpsi to the X vertex
          df3.getTrack(0).getPxPyPzGlo(pvecJpsi); // update momentum of Jpsi at the X vertex
          df3.getTrack(1).getPxPyPzGlo(pvecPos);  // momentum of pi+ at the X vertex
          df3.getTrack(2).getPxPyPzGlo(pvecNeg);  // momentum of pi- at the X vertex

          // get track impact parameters
          // This modifies track momenta!
          auto primaryVertex = getPrimaryVertex(collision);
          auto covMatrixPV = primaryVertex.getCov();
          hCovPVXX->Fill(covMatrixPV[0]);
          o2::dataformats::DCA impactParameter0;
          o2::dataformats::DCA impactParameter1;
          o2::dataformats::DCA impactParameter2;
          trackJpsi.propagateToDCA(primaryVertex, magneticField, &impactParameter0);
          trackParVarPos.propagateToDCA(primaryVertex, magneticField, &impactParameter1);
          trackParVarNeg.propagateToDCA(primaryVertex, magneticField, &impactParameter2);

          // get uncertainty of the decay length
          double phi, theta;
          getPointDirection(array{collision.posX(), collision.posY(), collision.posZ()}, XsecondaryVertex, phi, theta);
          auto errorDecayLength = std::sqrt(getRotatedCovMatrixXX(covMatrixPV, phi, theta) + getRotatedCovMatrixXX(covMatrixPCA, phi, theta));
          auto errorDecayLengthXY = std::sqrt(getRotatedCovMatrixXX(covMatrixPV, phi, 0.) + getRotatedCovMatrixXX(covMatrixPCA, phi, 0.));

          int hfFlag = 0;
          if (TESTBIT(jpsiCand.hfflag(), hf_cand_prong2::DecayType::JpsiToMuMu)) {
            SETBIT(hfFlag, hf_cand_x::DecayType::XToJpsiToMuMuPiPi); // dimuon channel
          }
          if (TESTBIT(jpsiCand.hfflag(), hf_cand_prong2::DecayType::JpsiToEE)) {
            SETBIT(hfFlag, hf_cand_x::DecayType::XToJpsiToEEPiPi); // dielectron channel
          }

          // fill the candidate table for the X here:
          rowCandidateBase(collision.globalIndex(),
                           collision.posX(), collision.posY(), collision.posZ(),
                           XsecondaryVertex[0], XsecondaryVertex[1], XsecondaryVertex[2],
                           errorDecayLength, errorDecayLengthXY,
                           chi2PCA,
                           pvecJpsi[0], pvecJpsi[1], pvecJpsi[2],
                           pvecPos[0], pvecPos[1], pvecPos[2],
                           pvecNeg[0], pvecNeg[1], pvecNeg[2],
                           impactParameter0.getY(), impactParameter1.getY(), impactParameter2.getY(),
                           std::sqrt(impactParameter0.getSigmaY2()), std::sqrt(impactParameter1.getSigmaY2()), std::sqrt(impactParameter2.getSigmaY2()),
                           jpsiCand.globalIndex(), trackPos.globalIndex(), trackNeg.globalIndex(),
                           hfFlag);

          // calculate invariant mass
          auto arrayMomenta = array{pvecJpsi, pvecPos, pvecNeg};
          massJpsiPiPi = RecoDecay::M(std::move(arrayMomenta), array{massJpsi, massPi, massPi});
          if (jpsiCand.isSelJpsiToEE() > 0) {
            hMassXToJpsiToEEPiPi->Fill(massJpsiPiPi);
          }
          if (jpsiCand.isSelJpsiToMuMu() > 0) {
            hMassXToJpsiToMuMuPiPi->Fill(massJpsiPiPi);
          }
        } // pi- loop
      }   // pi+ loop
    }     // Jpsi loop
  }       // process
};        // struct

/// Extends the base table with expression columns.
struct HFCandidateCreatorXExpressions {
  Spawns<aod::HfCandXExt> rowCandidateX;
  void init(InitContext const&) {}
};

/// Performs MC matching.
struct HFCandidateCreatorXMC {
  Produces<aod::HfCandXMCRec> rowMCMatchRec;
  Produces<aod::HfCandXMCGen> rowMCMatchGen;

  void process(aod::HfCandX const& candidates,
               aod::HfCandProng2,
               aod::BigTracksMC const& tracks,
               aod::McParticles const& particlesMC)
  {
    int indexRec = -1;
    int pdgCodeX = pdg::Code::kX3872;
    int pdgCodeJpsi = pdg::Code::kJpsi;
    int8_t sign = 0;
    int8_t flag = 0;
    int8_t origin = 0;
    int8_t channel = 0;

    // Match reconstructed candidates.
    for (auto& candidate : candidates) {
      //Printf("New rec. candidate");
      flag = 0;
      origin = 0;
      channel = 0;
      auto jpsiTrack = candidate.index0();
      auto daughterPosJpsi = jpsiTrack.index0_as<aod::BigTracksMC>();
      auto daughterNegJpsi = jpsiTrack.index1_as<aod::BigTracksMC>();
      auto arrayJpsiDaughters = array{daughterPosJpsi, daughterNegJpsi};
      auto arrayDaughters = array{candidate.index1_as<aod::BigTracksMC>(),
                                  candidate.index2_as<aod::BigTracksMC>(),
                                  daughterPosJpsi,
                                  daughterNegJpsi};

      // X → J/ψ π+ π-
      //Printf("Checking X → J/ψ π+ π-");
      indexRec = RecoDecay::getMatchedMCRec(particlesMC, arrayJpsiDaughters, pdg::Code::kJpsi, array{+kElectron, -kElectron}, true);
      if (indexRec > -1) {
        indexRec = RecoDecay::getMatchedMCRec(particlesMC, arrayDaughters, pdgCodeX, array{+kPiPlus, -kPiPlus, +kElectron, -kElectron}, true, &sign, 2);
        if (indexRec > -1) {
          flag = 1 << hf_cand_x::DecayType::XToJpsiToEEPiPi;
        }
      }

      if (flag == 0) {
        indexRec = RecoDecay::getMatchedMCRec(particlesMC, arrayJpsiDaughters, pdg::Code::kJpsi, array{+kMuonPlus, -kMuonPlus}, true);
        if (indexRec > -1) {
          indexRec = RecoDecay::getMatchedMCRec(particlesMC, arrayDaughters, pdgCodeX, array{+kPiPlus, -kPiPlus, +kMuonPlus, -kMuonPlus}, true, &sign, 2);
          if (indexRec > -1) {
            flag = 1 << hf_cand_x::DecayType::XToJpsiToMuMuPiPi;
          }
        }
      }

      // Check whether the particle is non-prompt (from a b quark).
      if (flag != 0) {
        auto particle = particlesMC.iteratorAt(indexRec);
        origin = (RecoDecay::getMother(particlesMC, particle, 5, true) > -1 ? NonPrompt : Prompt);
      }

      rowMCMatchRec(flag, origin, channel);
    }

    // Match generated particles.
    for (auto& particle : particlesMC) {
      //Printf("New gen. candidate");
      flag = 0;
      origin = 0;
      channel = 0;

      // X → J/ψ π+ π-
      //Printf("Checking X → J/ψ π+ π-");
      if (RecoDecay::isMatchedMCGen(particlesMC, particle, pdgCodeX, array{pdgCodeJpsi, +kPiPlus, -kPiPlus}, true)) {
        // Match J/psi --> e+e-
        std::vector<int> arrDaughter;
        RecoDecay::getDaughters(particlesMC, particle, &arrDaughter, array{pdgCodeJpsi}, 1);
        auto jpsiCandMC = particlesMC.iteratorAt(arrDaughter[0]);
        if (RecoDecay::isMatchedMCGen(particlesMC, jpsiCandMC, pdgCodeJpsi, array{+kElectron, -kElectron}, true)) {
          flag = 1 << hf_cand_x::DecayType::XToJpsiToEEPiPi;
        }

        if (flag == 0) {
          if (RecoDecay::isMatchedMCGen(particlesMC, jpsiCandMC, pdgCodeJpsi, array{+kMuonPlus, -kMuonPlus}, true)) {
            flag = 1 << hf_cand_x::DecayType::XToJpsiToMuMuPiPi;
          }
        }
      }

      // Check whether the particle is non-prompt (from a b quark).
      if (flag != 0) {
        origin = (RecoDecay::getMother(particlesMC, particle, 5, true) > -1 ? NonPrompt : Prompt);
      }

      rowMCMatchGen(flag, origin, channel);
    } // candidate loop
  }   // process
};    // struct

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  WorkflowSpec workflow{
    adaptAnalysisTask<HFCandidateCreatorX>(cfgc, TaskName{"hf-cand-creator-x"}),
    adaptAnalysisTask<HFCandidateCreatorXExpressions>(cfgc, TaskName{"hf-cand-creator-x-expressions"})};
  const bool doMC = cfgc.options().get<bool>("doMC");
  if (doMC) {
    workflow.push_back(adaptAnalysisTask<HFCandidateCreatorXMC>(cfgc, TaskName{"hf-cand-creator-x-mc"}));
  }
  return workflow;
}
