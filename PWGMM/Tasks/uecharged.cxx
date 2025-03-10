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
/// \author Antonio Ortiz (antonio.ortiz.velasquez@cern.ch)
/// \since November 2021

#include "Framework/runDataProcessing.h"
#include "Framework/AnalysisTask.h"
#include "Framework/AnalysisDataModel.h"
#include "Framework/ASoAHelpers.h"
#include "Common/DataModel/Multiplicity.h"
#include "Common/DataModel/EventSelection.h"
#include "Common/Core/TrackSelection.h"
#include "Common/Core/TrackSelectionDefaults.h"
#include "Common/DataModel/TrackSelectionTables.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/StaticFor.h"
#include <TH1F.h>
#include <TH2F.h>
#include <TF1.h>
#include <TRandom.h>
#include <cmath>

// TODO: implement flags for all, phys. sel., good vertex, ...

using namespace o2;
using namespace o2::framework;
using namespace o2::framework::expressions;

struct ueCharged {

  TrackSelection globalTrackswoPrim; // Track without cut for primaries
  TrackSelection globalTracks;       // Track with cut for primaries

  HistogramRegistry ue{"ue", {}, OutputObjHandlingPolicy::AnalysisObject, true, true};
  static constexpr std::string_view pNumDenMeasuredPS[3] = {"pNumDenMeasuredPS_NS", "pNumDenMeasuredPS_AS", "pNumDenMeasuredPS_TS"};
  static constexpr std::string_view pSumPtMeasuredPS[3] = {"pSumPtMeasuredPS_NS", "pSumPtMeasuredPS_AS", "pSumPtMeasuredPS_TS"};
  static constexpr std::string_view hPhi[3] = {"hPhi_NS", "hPhi_AS", "hPhi_TS"};
  // data driven correction
  static constexpr std::string_view hNumDenMCDd[3] = {"hNumDenMCDd_NS", "hNumDenMCDd_AS", "hNumDenMCDd_TS"};
  static constexpr std::string_view hSumPtMCDd[3] = {"hSumPtMCDd_NS", "hSumPtMCDd_AS", "hSumPtMCDd_TS"};
  static constexpr std::string_view hNumDenMCMatchDd[3] = {"hNumDenMCMatchDd_NS", "hNumDenMCMatchDd_AS", "hNumDenMCMatchDd_TS"};
  static constexpr std::string_view hSumPtMCMatchDd[3] = {"hSumPtMCMatchDd_NS", "hSumPtMCMatchDd_AS", "hSumPtMCMatchDd_TS"};
  // hist data for corrections
  static constexpr std::string_view hPtVsPtLeadingData[3] = {"hPtVsPtLeadingData_NS", "hPtVsPtLeadingData_AS", "hPtVsPtLeadingData_TS"};
  static constexpr std::string_view pNumDenData[3] = {"pNumDenData_NS", "pNumDenData_AS", "pNumDenData_TS"};
  static constexpr std::string_view pSumPtData[3] = {"pSumPtData_NS", "pSumPtData_AS", "pSumPtData_TS"};

  OutputObj<TF1> f_Eff{"fpara"};

  void init(o2::framework::InitContext&)
  {
    // primaries w/o golden cut
    globalTracks = getGlobalTrackSelection();
    globalTracks.SetRequireGoldenChi2(false);
    // all w/o golden cuts
    globalTrackswoPrim = getGlobalTrackSelection();
    globalTrackswoPrim.SetMaxDcaXYPtDep([](float pt) { return 3.f + pt; });
    globalTrackswoPrim.SetRequireGoldenChi2(false);

    ConfigurableAxis ptBinningt{"ptBinningt", {0, 0.15, 0.50, 1.00, 1.50, 2.00, 2.50, 3.00, 3.50, 4.00, 4.50, 5.00, 6.00, 7.00, 8.00, 9.00, 10.0, 12.0, 14.0, 16.0, 18.0, 20.0, 25.0, 30.0, 40.0, 50.0}, "pTtrig bin limits"};
    AxisSpec ptAxist = {ptBinningt, "#it{p}_{T}^{trig} (GeV/#it{c})"};

    ConfigurableAxis ptBinning{"ptBinning", {0, 0.0, 0.1, 0.15, 0.2, 0.25, 0.3, 0.35, 0.4, 0.45, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 12.0, 14.0, 16.0, 18.0, 20.0, 25.0, 30.0, 40.0, 50.0}, "pTassoc bin limits"};
    AxisSpec ptAxis = {ptBinning, "#it{p}_{T}^{assoc} (GeV/#it{c})"};

    f_Eff.setObject(new TF1("fpara", "(x<0.22)*((-0.770334)+(6.32178)*x)+(x>=0.22&&x<0.4)*((0.310721)+(2.02610)*x+(-2.25005)*x*x)+(x>=0.4&&x<1.0)*((1.21232)+(-1.27511)*x+(0.588435)*x*x)+(x>=1.0&&x<5.5)*((0.502911)+(0.0416893)*x)+(x>=5.5)*(0.709143)", 0.15, 50.0));

    ue.add("hStat", "TotalEvents", HistType::kTH1F, {{1, 0.5, 1.5, " "}});
    ue.add("hdNdeta", "dNdeta", HistType::kTH1F, {{50, -2.5, 2.5, " "}});
    ue.add("vtxZEta", ";#eta;vtxZ", HistType::kTH2F, {{50, -2.5, 2.5, " "}, {60, -30, 30, " "}});
    ue.add("phiEta", ";#eta;#varphi", HistType::kTH2F, {{50, -2.5, 2.5}, {200, 0., 2 * M_PI, " "}});
    ue.add("hvtxZ", "vtxZ", HistType::kTH1F, {{40, -20.0, 20.0, " "}});

    ue.add("hCounter", "Counter; sel; Nev", HistType::kTH1D, {{3, 0, 3, " "}});
    ue.add("hPtLeadingRecPS", "rec pTleading after physics selection", HistType::kTH1D, {ptAxist});

    for (int i = 0; i < 3; ++i) {
      ue.add(pNumDenMeasuredPS[i].data(), "Number Density; ; #LT #it{N}_{trk} #GT", HistType::kTProfile, {ptAxist});
      ue.add(pSumPtMeasuredPS[i].data(), "Total #it{p}_{T}; ; #LT#sum#it{p}_{T}#GT", HistType::kTProfile, {ptAxist});
      ue.add(hPhi[i].data(), "all charged; #Delta#phi; Counts", HistType::kTH1D, {{64, -M_PI / 2.0, 3.0 * M_PI / 2.0, ""}});
    }
    // Data driven
    for (int i = 0; i < 3; ++i) {
      ue.add(hNumDenMCDd[i].data(), " ", HistType::kTH2D, {{ptAxist}, {100, -0.5, 99.5, "#it{N}_{trk}"}});
      ue.add(hSumPtMCDd[i].data(), " ", HistType::kTH2D, {{ptAxist}, {ptAxis}});
      ue.add(hNumDenMCMatchDd[i].data(), " ", HistType::kTH2D, {{ptAxist}, {100, -0.5, 99.5, "#it{N}_{trk}"}});
      ue.add(hSumPtMCMatchDd[i].data(), " ", HistType::kTH2D, {{ptAxist}, {ptAxis}});
    }

    for (int i = 0; i < 3; ++i) {
      ue.add(hPtVsPtLeadingData[i].data(), " ", HistType::kTH2D, {{ptAxist}, {ptAxis}});
      ue.add(pNumDenData[i].data(), "", HistType::kTProfile, {ptAxist});
      ue.add(pSumPtData[i].data(), "", HistType::kTProfile, {ptAxist});
    }
    ue.add("hPtLeadingData", " ", HistType::kTH1D, {{ptAxist}});
    ue.add("hPTVsDCAData", " ", HistType::kTH2D, {{ptAxis}, {121, -3.025, 3.025, "#it{DCA}_{xy} (cm)"}});
  }

  float DeltaPhi(float phia, float phib,
                 float rangeMin = -M_PI / 2.0, float rangeMax = 3.0 * M_PI / 2.0)
  {
    float dphi = -999;

    if (phia < 0) {
      phia += 2 * M_PI;
    } else if (phia > 2 * M_PI) {
      phia -= 2 * M_PI;
    }
    if (phib < 0) {
      phib += 2 * M_PI;
    } else if (phib > 2 * M_PI) {
      phib -= 2 * M_PI;
    }
    dphi = phib - phia;
    if (dphi < rangeMin) {
      dphi += 2 * M_PI;
    } else if (dphi > rangeMax) {
      dphi -= 2 * M_PI;
    }

    return dphi;
  }

  void process(soa::Join<aod::Collisions, aod::EvSels>::iterator const& collision, soa::Join<aod::Tracks, aod::TracksExtra, aod::TracksExtended> const& tracks)
  {

    ue.fill(HIST("hCounter"), 0);
    //check if your trigger alias is fired
    if (!collision.alias()[kINT7]) {
      return;
    }

    ue.fill(HIST("hCounter"), 1);

    ue.fill(HIST("hStat"), collision.size());
    //hStat->Fill(collision.size());
    if (TMath::Abs(collision.posZ()) > 10.0) {
      return;
    }
    auto vtxZ = collision.posZ();

    ue.fill(HIST("hCounter"), 2);

    ue.fill(HIST("hvtxZ"), vtxZ);
    // loop over selected tracks
    double flPt = 0; // leading pT
    double flPhi = 0;
    int flIndex = 0;
    for (auto& track : tracks) {

      if (!globalTracks.IsSelected(track)) {
        continue;
      }

      ue.fill(HIST("hdNdeta"), track.eta());
      ue.fill(HIST("vtxZEta"), track.eta(), vtxZ);
      ue.fill(HIST("phiEta"), track.eta(), track.phi());

      if (track.pt() < 0.15) {
        continue;
      }
      if (flPt < track.pt()) {
        flPt = track.pt();
        flPhi = track.phi();
        flIndex = track.globalIndex();
      }
    }
    ue.fill(HIST("hPtLeadingRecPS"), flPt);
    vector<double> ue_rec;
    ue_rec.clear();
    int nchm_top[3];
    double sumptm_top[3];
    for (int i = 0; i < 3; ++i) {
      nchm_top[i] = 0;
      sumptm_top[i] = 0;
    }
    //LOGF(info, "--------------------   FLAG 0!!!!!!!!!!!");
    vector<Float_t> ptArray;
    vector<Float_t> phiArray;
    vector<int> indexArray;

    for (auto& track : tracks) {

      if (track.pt() < 0.15) {
        continue;
      }

      if (globalTrackswoPrim.IsSelected(track)) {
        ue.fill(HIST("hPTVsDCAData"), track.pt(), track.dcaXY());
      }

      if (!globalTracks.IsSelected(track)) {
        continue;
      }

      // applying the efficiency twice for the misrec of leading particle
      if (f_Eff->Eval(track.pt()) > gRandom->Uniform(0, 1)) {
        ptArray.push_back(track.pt());
        phiArray.push_back(track.phi());
        indexArray.push_back(track.globalIndex());
      }

      // remove the autocorrelation
      if (flIndex == track.globalIndex()) {
        continue;
      }

      double DPhi = DeltaPhi(track.phi(), flPhi);

      // definition of the topological regions
      if (TMath::Abs(DPhi) < M_PI / 3.0) { // near side
        ue.fill(HIST(hPhi[0]), DPhi);
        ue.fill(HIST(hPtVsPtLeadingData[0]), flPt, track.pt());
        nchm_top[0]++;
        sumptm_top[0] += track.pt();
      } else if (TMath::Abs(DPhi - M_PI) < M_PI / 3.0) { // away side
        ue.fill(HIST(hPhi[1]), DPhi);
        ue.fill(HIST(hPtVsPtLeadingData[1]), flPt, track.pt());
        nchm_top[1]++;
        sumptm_top[1] += track.pt();
      } else { // transverse side
        ue.fill(HIST(hPhi[2]), DPhi);
        ue.fill(HIST(hPtVsPtLeadingData[2]), flPt, track.pt());
        nchm_top[2]++;
        sumptm_top[2] += track.pt();
      }
    }

    for (int i_reg = 0; i_reg < 3; ++i_reg) {
      ue_rec.push_back(1.0 * nchm_top[i_reg]);
    }
    for (int i_reg = 0; i_reg < 3; ++i_reg) {
      ue_rec.push_back(sumptm_top[i_reg]);
    }

    // add flags for Vtx, PS, ev sel
    static_for<0, 2>([&](auto i) {
      constexpr int index = i.value;
      ue.fill(HIST(pNumDenMeasuredPS[index]), flPt, ue_rec[index]);
      ue.fill(HIST(pNumDenData[index]), flPt, ue_rec[index]);
      ue.fill(HIST(pSumPtMeasuredPS[index]), flPt, ue_rec[index + 3]);
      ue.fill(HIST(pSumPtData[index]), flPt, ue_rec[index + 3]);
    });

    ue.fill(HIST("hPtLeadingData"), flPt);

    // Compute data driven (DD) missidentification correction
    Float_t flPtdd = 0; // leading pT
    Float_t flPhidd = 0;
    int flIndexdd = 0;
    int ntrkdd = ptArray.size();

    for (int i = 0; i < ntrkdd; ++i) {
      if (flPtdd < ptArray[i]) {
        flPtdd = ptArray[i];
        flPhidd = phiArray[i];
        flIndexdd = indexArray[i];
      }
    }
    int nchm_topdd[3];
    double sumptm_topdd[3];
    for (int i = 0; i < 3; ++i) {
      nchm_topdd[i] = 0;
      sumptm_topdd[i] = 0;
    }
    for (int i = 0; i < ntrkdd; ++i) {
      if (indexArray[i] == flIndexdd) {
        continue;
      }
      double DPhi = DeltaPhi(phiArray[i], flPhidd);
      if (TMath::Abs(DPhi) < M_PI / 3.0) { // near side
        nchm_topdd[0]++;
        sumptm_topdd[0] += ptArray[i];
      } else if (TMath::Abs(DPhi - M_PI) < M_PI / 3.0) { // away side
        nchm_topdd[1]++;
        sumptm_topdd[1] += ptArray[i];
      } else { // transverse side
        nchm_topdd[2]++;
        sumptm_topdd[2] += ptArray[i];
      }
    }

    static_for<0, 2>([&](auto i) {
      constexpr int index = i.value;
      ue.fill(HIST(hNumDenMCDd[index]), flPtdd, nchm_topdd[index]);
      ue.fill(HIST(hSumPtMCDd[index]), flPtdd, sumptm_topdd[index]);
    });

    if (flIndexdd == flIndex) {
      static_for<0, 2>([&](auto i) {
        constexpr int index = i.value;
        ue.fill(HIST(hNumDenMCMatchDd[index]), flPtdd, nchm_topdd[index]);
        ue.fill(HIST(hSumPtMCMatchDd[index]), flPtdd, sumptm_topdd[index]);
      });
    }

    ptArray.clear();
    phiArray.clear();
    indexArray.clear();
  }
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  WorkflowSpec workflow{};
  workflow.push_back(adaptAnalysisTask<ueCharged>(cfgc));
  return workflow;
}
