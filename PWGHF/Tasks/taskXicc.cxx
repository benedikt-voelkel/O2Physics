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

/// \file taskXicc.cxx
/// \brief Ξcc±± analysis task
/// \note Inspired from taskLc.cxx
///
/// \author Gian Michele Innocenti <gian.michele.innocenti@cern.ch>, CERN
/// \author Jinjoo Seo <jin.joo.seo@cern.ch>, Inha University

#include "Framework/AnalysisTask.h"
#include "Framework/HistogramRegistry.h"
#include "PWGHF/DataModel/HFSecondaryVertex.h"
#include "PWGHF/DataModel/HFCandidateSelectionTables.h"

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;
using namespace o2::aod::hf_cand_xicc;
//using namespace o2::aod::hf_cand_prong3;

void customize(std::vector<o2::framework::ConfigParamSpec>& workflowOptions)
{
  ConfigParamSpec optionDoMC{"doMC", VariantType::Bool, true, {"Fill MC histograms."}};
  workflowOptions.push_back(optionDoMC);
}

#include "Framework/runDataProcessing.h"

/// Ξcc±± analysis task
struct HfTaskXicc {
  HistogramRegistry registry{
    "registry",
    {{"hPtCand", "#Xi^{++}_{cc}-candidates;candidate #it{p}_{T} (GeV/#it{c});entries", {HistType::kTH1F, {{360, 0., 36.}}}},
     {"hPtProng0", "#Xi^{++}_{cc}-candidates;prong 0 #it{p}_{T} (GeV/#it{c});entries", {HistType::kTH1F, {{360, 0., 36.}}}},
     {"hPtProng1", "#Xi^{++}_{cc}-candidates;prong 1 #it{p}_{T} (GeV/#it{c});entries", {HistType::kTH1F, {{360, 0., 36.}}}}}};

  Configurable<int> d_selectionFlagXicc{"d_selectionFlagXicc", 1, "Selection Flag for Xicc"};
  Configurable<double> cutYCandMax{"cutYCandMax", -1., "max. cand. rapidity"};
  Configurable<std::vector<double>> bins{"pTBins", std::vector<double>{hf_cuts_xicc_topkpipi::pTBins_v}, "pT bin limits"};

  void init(o2::framework::InitContext&)
  {
    auto vbins = (std::vector<double>)bins;
    registry.add("hMass", "#Xi^{++}_{cc} candidates;inv. mass (p K #pi #pi) (GeV/#it{c}^{2});entries", {HistType::kTH2F, {{400, 3.2, 4.0}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hDecLength", "#Xi^{++}_{cc} candidates;decay length (cm);entries", {HistType::kTH2F, {{500, 0., 0.05}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hChi2PCA", "#Xi^{++}_{cc} candidates;chi2 PCA (cm);entries", {HistType::kTH2F, {{500, 0., 0.01}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hd0Prong0", "#Xi^{++}_{cc} candidates;prong 0 DCAxy to prim. vertex (cm);entries", {HistType::kTH2F, {{200, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hd0Prong1", "#Xi^{++}_{cc} candidates;prong 1 DCAxy to prim. vertex (cm);entries", {HistType::kTH2F, {{200, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hd0d0", "#Xi^{++}_{cc} candidates;product of DCAxy to prim. vertex (cm^{2}); #it{p}_{T} (GeV/#it{c}); entries", {HistType::kTH2F, {{500, -0.05, 0.05}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hCt", "#Xi^{++}_{cc} candidates;proper lifetime (#Xi^{++}_{cc}) * #it{c} (cm);entries", {HistType::kTH2F, {{100, 0., 0.1}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hCPA", "#Xi^{++}_{cc} candidates;cosine of pointing angle;entries", {HistType::kTH2F, {{2200, -1.1, 1.1}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hEta", "#Xi^{++}_{cc} candidates;candidate #it{#eta};entries", {HistType::kTH2F, {{250, -5., 5.}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hY", "#Xi^{++}_{cc} candidates;candidate rapidity;entries", {HistType::kTH2F, {{250, -5., 5.}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hSelectionStatus", "#Xi^{++}_{cc} candidates;selection status;entries", {HistType::kTH2F, {{5, -0.5, 4.5}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hImpParErr0", "#Xi^{++}_{cc} candidates;impact parameter error (cm);entries", {HistType::kTH2F, {{200, 0, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hImpParErr1", "#Xi^{++}_{cc} candidates;impact parameter error (cm);entries", {HistType::kTH2F, {{200, 0, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hDecLenErr", "#Xi^{++}_{cc} candidates;decay length error (cm);entries", {HistType::kTH2F, {{100, 0., 1.}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
  }

  Filter filterSelectCandidates = (aod::hf_selcandidate_xicc::isSelXiccToPKPiPi >= d_selectionFlagXicc);
  void process(soa::Filtered<soa::Join<aod::HfCandXicc, aod::HFSelXiccToPKPiPiCandidate>> const& candidates)
  //void process(aod::HfCandXicc const& candidates)
  {
    for (auto& candidate : candidates) {
      if (!(candidate.hfflag() & 1 << DecayType::XiccToXicPi)) {
        continue;
      }
      if (cutYCandMax >= 0. && std::abs(YXicc(candidate)) > cutYCandMax) {
        continue;
      }
      registry.fill(HIST("hMass"), InvMassXiccToXicPi(candidate), candidate.pt()); //FIXME need to consider the two mass hp
      registry.fill(HIST("hPtCand"), candidate.pt());
      registry.fill(HIST("hPtProng0"), candidate.ptProng0());
      registry.fill(HIST("hPtProng1"), candidate.ptProng1());
      registry.fill(HIST("hd0d0"), candidate.impactParameterProduct(), candidate.pt());
      registry.fill(HIST("hDecLength"), candidate.decayLength(), candidate.pt());
      registry.fill(HIST("hChi2PCA"), candidate.chi2PCA(), candidate.pt());
      registry.fill(HIST("hd0Prong0"), candidate.impactParameter0(), candidate.pt());
      registry.fill(HIST("hd0Prong1"), candidate.impactParameter1(), candidate.pt());
      registry.fill(HIST("hCt"), CtXicc(candidate), candidate.pt());
      registry.fill(HIST("hCPA"), candidate.cpa(), candidate.pt());
      registry.fill(HIST("hEta"), candidate.eta(), candidate.pt());
      registry.fill(HIST("hY"), YXicc(candidate), candidate.pt());
      registry.fill(HIST("hSelectionStatus"), candidate.isSelXiccToPKPiPi(), candidate.pt());
      registry.fill(HIST("hImpParErr0"), candidate.errorImpactParameter0(), candidate.pt());
      registry.fill(HIST("hImpParErr1"), candidate.errorImpactParameter1(), candidate.pt());
      registry.fill(HIST("hDecLenErr"), candidate.errorDecayLength(), candidate.pt());
    }
  }
};

/// Fills MC histograms.
struct HfTaskXiccMc {
  HistogramRegistry registry{
    "registry",
    {{"hPtRecSig", "#Xi^{++}_{cc} candidates (rec. matched);#it{p}_{T} (GeV/#it{c});entries", {HistType::kTH1F, {{360, 0., 36.}}}},
     {"hPtRecBg", "#Xi^{++}_{cc} candidates (rec. unmatched);#it{p}_{T} (GeV/#it{c});entries", {HistType::kTH1F, {{360, 0., 36.}}}},
     {"hPtGen", "#Xi^{++}_{cc} MC particles (matched);#it{p}_{T} (GeV/#it{c});entries", {HistType::kTH1F, {{360, 0., 36.}}}},
     {"hPtGenSig", "#Xi^{++}_{cc} candidates (rec. matched);#it{p}_{T} (GeV/#it{c});entries", {HistType::kTH1F, {{360, 0., 36.}}}},
     {"hEtaRecSig", "#Xi^{++}_{cc} candidates (rec. matched);#it{#eta};entries", {HistType::kTH1F, {{250, -5., 5.}}}},
     {"hEtaRecBg", "#Xi^{++}_{cc} candidates (rec. unmatched);#it{#eta};entries", {HistType::kTH1F, {{250, -5., 5.}}}},
     {"hEtaGen", "#Xi^{++}_{cc} MC particles (matched);#it{#eta};entries", {HistType::kTH1F, {{250, -5., 5.}}}},
     {"hYRecSig", "#Xi^{++}_{cc} candidates (rec. matched);#it{y};entries", {HistType::kTH1F, {{250, -5., 5.}}}},
     {"hYRecBg", "#Xi^{++}_{cc} candidates (rec. unmatched);#it{y};entries", {HistType::kTH1F, {{250, -5., 5.}}}},
     {"hYGen", "#Xi^{++}_{cc} MC particles (matched);#it{y};entries", {HistType::kTH1F, {{250, -5., 5.}}}},
     {"hPtvsEtavsYGen", "#Xi^{++}_{cc} MC particles (matched);#it{p}_{T} (GeV/#it{c});#it{#eta};#it{y}", {HistType::kTH3F, {{360, 0., 36.}, {250, -5., 5.}, {20, -5., 5.}}}}}};

  Configurable<int> d_selectionFlagXicc{"d_selectionFlagXicc", 1, "Selection Flag for Xicc"};
  Configurable<double> cutYCandMax{"cutYCandMax", -1., "max. cand. rapidity"};
  Configurable<std::vector<double>> bins{"pTBins", std::vector<double>{hf_cuts_xicc_topkpipi::pTBins_v}, "pT bin limits"};

  void init(o2::framework::InitContext&)
  {
    auto vbins = (std::vector<double>)bins;
    registry.add("hMassVsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;inv. mass (p K #pi #pi) (GeV/#it{c}^{2});entries", {HistType::kTH2F, {{400, 3.2, 4.0}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hMassVsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;inv. mass (p K #pi #pi) (GeV/#it{c}^{2});entries", {HistType::kTH2F, {{400, 3.2, 4.0}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hChi2PCAVsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;chi2 PCA (cm);entries", {HistType::kTH2F, {{500, 0., 0.01}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hChi2PCAVsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;chi2 PCA (cm);entries", {HistType::kTH2F, {{500, 0., 0.01}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hDecLengthVsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;decay length (cm);entries", {HistType::kTH2F, {{500, 0., 0.05}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hDecLengthVsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;decay length (cm);entries", {HistType::kTH2F, {{500, 0., 0.05}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hd0Prong0VsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;prong 0 DCAxy to prim. vertex (cm);entries", {HistType::kTH2F, {{200, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hd0Prong0VsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;prong 0 DCAxy to prim. vertex (cm);entries", {HistType::kTH2F, {{200, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hd0Prong1VsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;prong 1 DCAxy to prim. vertex (cm);entries", {HistType::kTH2F, {{200, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hd0Prong1VsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;prong 1 DCAxy to prim. vertex (cm);entries", {HistType::kTH2F, {{200, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hd0d0VsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;product of DCAxy to prim. vertex (cm^{2}); #it{p}_{T} (GeV/#it{c}); entries", {HistType::kTH2F, {{500, -0.05, 0.05}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hd0d0VsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;product of DCAxy to prim. vertex (cm^{2}); #it{p}_{T} (GeV/#it{c}); entries", {HistType::kTH2F, {{500, -0.05, 0.05}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hCtVsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;proper lifetime (#Xi_{cc}) * #it{c} (cm);entries", {HistType::kTH2F, {{100, 0., 0.1}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hCtVsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;proper lifetime (#Xi_{cc}) * #it{c} (cm);entries", {HistType::kTH2F, {{100, 0., 0.1}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hCPAVsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;cosine of pointing angle;entries", {HistType::kTH2F, {{2200, -1.1, 1.1}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hCPAVsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;cosine of pointing angle;entries", {HistType::kTH2F, {{2200, -1.1, 1.1}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hEtaVsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;candidate #it{#eta};entries", {HistType::kTH2F, {{250, -5., 5.}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hEtaVsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;candidate #it{#eta};entries", {HistType::kTH2F, {{250, -5., 5.}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hYVsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;candidate rapidity;entries", {HistType::kTH2F, {{250, -5., 5.}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hYVsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;candidate rapidity;entries", {HistType::kTH2F, {{250, -5., 5.}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hImpParErr0VsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;impact parameter error (cm);entries", {HistType::kTH2F, {{200, 0, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hImpParErr0VsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;impact parameter error (cm);entries", {HistType::kTH2F, {{200, 0, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hImpParErr1VsPtRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;impact parameter error (cm);entries", {HistType::kTH2F, {{200, 0, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hImpParErr1VsPtRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;impact parameter error (cm);entries", {HistType::kTH2F, {{200, 0, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    // resolutions
    registry.add("hXSecVtxPosRecGenDiffSig", "#Xi^{++}_{cc} (rec. matched) candidates;x-axis sec. vertex pos. reco - gen (cm);entries", {HistType::kTH2F, {{400, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hYSecVtxPosRecGenDiffSig", "#Xi^{++}_{cc} (rec. matched) candidates;y-axis sec. vertex pos. reco - gen (cm);entries", {HistType::kTH2F, {{400, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hZSecVtxPosRecGenDiffSig", "#Xi^{++}_{cc} (rec. matched) candidates;z-axis sec. vertex pos. reco - gen (cm);entries", {HistType::kTH2F, {{400, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    registry.add("hPtRecGenDiffSig", "#Xi^{++}_{cc} (rec. matched) candidates;pt reco - gen;entries (GeV/#it{c}})", {HistType::kTH2F, {{400, -1.0, 1.0}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    //debug
    registry.add("hDebugMCmatching", "#Xi^{++}_{cc} (rec. matched) candidates;debug MC matching bitmap;entries", {HistType::kTH2F, {{5, -0.5, 4.5}, {vbins, "#it{p}_{T} (GeV/#it{c})"}}});
    // Check Y dependence (To be removed)
    registry.add("hMassVsPtVsYRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;inv. mass (p K #pi #pi) (GeV/#it{c}^{2}); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{400, 3.2, 4.0}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hMassVsPtVsYRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;inv. mass (p K #pi #pi) (GeV/#it{c}^{2}); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{400, 3.2, 4.0}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hChi2PCAVsPtVsYRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;chi2 PCA (cm); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{500, 0., 0.01}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hChi2PCAVsPtVsYRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;chi2 PCA (cm); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{500, 0., 0.01}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hDecLengthVsPtVsYRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;decay length (cm); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{500, 0., 0.05}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hDecLengthVsPtVsYRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;decay length (cm); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{500, 0., 0.05}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hd0Prong0VsPtVsYRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;prong 0 DCAxy to prim. vertex (cm); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{200, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hd0Prong0VsPtVsYRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;prong 0 DCAxy to prim. vertex (cm); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{200, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hd0Prong1VsPtVsYRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;prong 1 DCAxy to prim. vertex (cm); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{200, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hd0Prong1VsPtVsYRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;prong 1 DCAxy to prim. vertex (cm); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{200, -0.02, 0.02}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hCtVsPtVsYRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;proper lifetime (#Xi_{cc}) * #it{c} (cm); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{100, 0., 0.1}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hCtVsPtVsYRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;proper lifetime (#Xi_{cc}) * #it{c} (cm); #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{100, 0., 0.1}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hCPAVsPtVsYRecSig", "#Xi^{++}_{cc} (rec. matched) candidates;cosine of pointing angle; #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{2200, -1.1, 1.1}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
    registry.add("hCPAVsPtVsYRecBg", "#Xi^{++}_{cc} (rec. unmatched) candidates;cosine of pointing angle; #it{p}_{T} (GeV/#it{c}); #it{y}", {HistType::kTH3F, {{2200, -1.1, 1.1}, {vbins, "#it{p}_{T} (GeV/#it{c})"}, {20, -5., 5.}}});
  }

  Filter filterSelectCandidates = (aod::hf_selcandidate_xicc::isSelXiccToPKPiPi >= d_selectionFlagXicc);
  //void process(soa::Filtered<soa::Join<aod::HfCandXicc, aod::HFSelXiccToPKPiPiCandidate>> const& candidates)
  void process(soa::Filtered<soa::Join<aod::HfCandXicc, aod::HFSelXiccToPKPiPiCandidate, aod::HfCandXiccMCRec>> const& candidates,
               soa::Join<aod::McParticles, aod::HfCandXiccMCGen> const& particlesMC, aod::BigTracksMC const& tracks)
  {
    // MC rec.
    //Printf("MC Candidates: %d", candidates.size());
    for (auto& candidate : candidates) {
      if (!(candidate.hfflag() & 1 << DecayType::XiccToXicPi)) {
        continue;
      }
      if (cutYCandMax >= 0. && std::abs(YXicc(candidate)) > cutYCandMax) {
        continue;
      }
      if (std::abs(candidate.flagMCMatchRec()) == 1 << DecayType::XiccToXicPi) {
        // Get the corresponding MC particle.
        auto indexMother = RecoDecay::getMother(particlesMC, candidate.index1_as<aod::BigTracksMC>().mcParticle_as<soa::Join<aod::McParticles, aod::HfCandXiccMCGen>>(), 4422, true);
        auto particleXicc = particlesMC.iteratorAt(indexMother);
        auto particleXic = particlesMC.iteratorAt(particleXicc.daughter0Id());
        /*
        auto daughter1 = particlesMC.iteratorAt(particleXicc.daughter1());
        auto p0xic = particlesMC.iteratorAt(particleXic.daughter0());
        auto p1xic = particlesMC.iteratorAt(particleXic.daughter0()+1);
        auto p2xic = particlesMC.iteratorAt(particleXic.daughter1());
        LOGF(info, "mother pdg %d", particleXicc.pdgCode());
        LOGF(info, "Xic pdg %d", particleXic.pdgCode());
        LOGF(info, "Xic prong 0 pdg %d", p0xic.pdgCode());
        LOGF(info, "Xic prong 1 pdg %d", p1xic.pdgCode());
        LOGF(info, "Xic prong 2 pdg %d", p2xic.pdgCode());
        LOGF(info, "Pion pdg %d", daughter1.pdgCode());
*/
        registry.fill(HIST("hPtGenSig"), particleXicc.pt()); // gen. level pT
        registry.fill(HIST("hPtRecSig"), candidate.pt());    // rec. level pT
        registry.fill(HIST("hEtaRecSig"), candidate.eta());
        registry.fill(HIST("hYRecSig"), YXicc(candidate));
        registry.fill(HIST("hMassVsPtRecSig"), InvMassXiccToXicPi(candidate), candidate.pt()); //FIXME need to consider the two mass hp
        registry.fill(HIST("hDecLengthVsPtRecSig"), candidate.decayLength(), candidate.pt());
        registry.fill(HIST("hChi2PCAVsPtRecSig"), candidate.chi2PCA(), candidate.pt());
        registry.fill(HIST("hCPAVsPtRecSig"), candidate.cpa(), candidate.pt());
        registry.fill(HIST("hd0Prong0VsPtRecSig"), candidate.impactParameter0(), candidate.pt());
        registry.fill(HIST("hd0Prong1VsPtRecSig"), candidate.impactParameter1(), candidate.pt());
        registry.fill(HIST("hd0d0VsPtRecSig"), candidate.impactParameterProduct(), candidate.pt());
        registry.fill(HIST("hCtVsPtRecSig"), CtXicc(candidate), candidate.pt());
        registry.fill(HIST("hEtaVsPtRecSig"), candidate.eta(), candidate.pt());
        registry.fill(HIST("hYVsPtRecSig"), YXicc(candidate), candidate.pt());
        registry.fill(HIST("hImpParErr0VsPtRecSig"), candidate.errorImpactParameter0(), candidate.pt());
        registry.fill(HIST("hImpParErr1VsPtRecSig"), candidate.errorImpactParameter1(), candidate.pt());
        registry.fill(HIST("hXSecVtxPosRecGenDiffSig"), candidate.xSecondaryVertex() - particleXic.vx(), candidate.pt());
        registry.fill(HIST("hYSecVtxPosRecGenDiffSig"), candidate.ySecondaryVertex() - particleXic.vy(), candidate.pt());
        registry.fill(HIST("hZSecVtxPosRecGenDiffSig"), candidate.zSecondaryVertex() - particleXic.vz(), candidate.pt());
        registry.fill(HIST("hPtRecGenDiffSig"), candidate.pt() - particleXicc.pt(), candidate.pt());
        // Check Y dependence (To be removed)
        registry.fill(HIST("hMassVsPtVsYRecSig"), InvMassXiccToXicPi(candidate), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hDecLengthVsPtVsYRecSig"), candidate.decayLength(), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hChi2PCAVsPtVsYRecSig"), candidate.chi2PCA(), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hCPAVsPtVsYRecSig"), candidate.cpa(), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hd0Prong0VsPtVsYRecSig"), candidate.impactParameter0(), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hd0Prong1VsPtVsYRecSig"), candidate.impactParameter1(), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hCtVsPtVsYRecSig"), CtXicc(candidate), candidate.pt(), YXicc(candidate));
      } else {
        registry.fill(HIST("hPtRecBg"), candidate.pt());
        registry.fill(HIST("hEtaRecBg"), candidate.eta());
        registry.fill(HIST("hYRecBg"), YXicc(candidate));
        registry.fill(HIST("hMassVsPtRecBg"), InvMassXiccToXicPi(candidate), candidate.pt()); //FIXME need to consider the two mass hp
        registry.fill(HIST("hDecLengthVsPtRecBg"), candidate.decayLength(), candidate.pt());
        registry.fill(HIST("hChi2PCAVsPtRecBg"), candidate.chi2PCA(), candidate.pt());
        registry.fill(HIST("hCPAVsPtRecBg"), candidate.cpa(), candidate.pt());
        registry.fill(HIST("hd0Prong0VsPtRecBg"), candidate.impactParameter0(), candidate.pt());
        registry.fill(HIST("hd0Prong1VsPtRecBg"), candidate.impactParameter1(), candidate.pt());
        registry.fill(HIST("hd0d0VsPtRecBg"), candidate.impactParameterProduct(), candidate.pt());
        registry.fill(HIST("hCtVsPtRecBg"), CtXicc(candidate), candidate.pt());
        registry.fill(HIST("hEtaVsPtRecBg"), candidate.eta(), candidate.pt());
        registry.fill(HIST("hYVsPtRecBg"), YXicc(candidate), candidate.pt());
        registry.fill(HIST("hImpParErr0VsPtRecBg"), candidate.errorImpactParameter0(), candidate.pt());
        registry.fill(HIST("hImpParErr1VsPtRecBg"), candidate.errorImpactParameter1(), candidate.pt());
        registry.fill(HIST("hDebugMCmatching"), candidate.debugMCRec(), candidate.pt());
        // Check Y dependence (To be removed)
        registry.fill(HIST("hMassVsPtVsYRecBg"), InvMassXiccToXicPi(candidate), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hDecLengthVsPtVsYRecBg"), candidate.decayLength(), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hChi2PCAVsPtVsYRecBg"), candidate.chi2PCA(), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hCPAVsPtVsYRecBg"), candidate.cpa(), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hd0Prong0VsPtVsYRecBg"), candidate.impactParameter0(), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hd0Prong1VsPtVsYRecBg"), candidate.impactParameter1(), candidate.pt(), YXicc(candidate));
        registry.fill(HIST("hCtVsPtVsYRecBg"), CtXicc(candidate), candidate.pt(), YXicc(candidate));
      }
    } // end of loop over reconstructed candidates
    // MC gen.
    //Printf("MC Particles: %d", particlesMC.size());
    for (auto& particle : particlesMC) {
      if (std::abs(particle.flagMCMatchGen()) == 1 << DecayType::XiccToXicPi) {
        if (cutYCandMax >= 0. && std::abs(RecoDecay::Y(array{particle.px(), particle.py(), particle.pz()}, RecoDecay::getMassPDG(particle.pdgCode()))) > cutYCandMax) {
          continue;
        }
        registry.fill(HIST("hPtGen"), particle.pt());
        registry.fill(HIST("hEtaGen"), particle.eta());
        registry.fill(HIST("hYGen"), RecoDecay::Y(array{particle.px(), particle.py(), particle.pz()}, RecoDecay::getMassPDG(particle.pdgCode())));
        registry.fill(HIST("hPtvsEtavsYGen"), particle.pt(), particle.eta(), RecoDecay::Y(array{particle.px(), particle.py(), particle.pz()}, RecoDecay::getMassPDG(particle.pdgCode())));
      }
    } // end of loop of MC particles
  }   // end of process function
};    // end of struct

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  WorkflowSpec workflow{
    adaptAnalysisTask<HfTaskXicc>(cfgc)};
  const bool doMC = cfgc.options().get<bool>("doMC");
  if (doMC) {
    workflow.push_back(adaptAnalysisTask<HfTaskXiccMc>(cfgc));
  }
  return workflow;
}
