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

/// \file taskCharmPolarisation.cxx
/// \brief Analysis task for non-scalar charm hadron polarisation
///
/// \author F. Grosa (CERN) fabrizio.grosa@cern.ch
/// \author S. Kundu (CERN) sourav.kundu@cern.ch
/// \author M. Faggin (CERN) mattia.faggin@cern.ch

#include "TRandom3.h"
#include "Math/Vector3D.h"
#include "Math/Vector4D.h"
#include "Math/GenVector/Boost.h"

#include "Framework/AnalysisTask.h"
#include "Framework/HistogramRegistry.h"
#include "Framework/runDataProcessing.h"

// #include "Common/Core/EventPlaneHelper.h"
// #include "Common/DataModel/Qvectors.h"

#include "PWGHF/Core/HfHelper.h"
#include "PWGHF/DataModel/CandidateSelectionTables.h"
#include "PWGHF/DataModel/CandidateReconstructionTables.h"

using namespace o2;
using namespace o2::aod;
using namespace o2::framework;
using namespace o2::framework::expressions;

namespace o2::aod
{
namespace charm_polarisation
{
enum DecayChannel : uint8_t {
  DstarToDzeroPi = 0,
  LcToPKPi,
  LcToPK0S
};
enum MassHyposLcToPKPi : uint8_t {
  PKPi = 0,
  PiKP,
  NMassHypoLcToPKPi
};
} // namespace charm_polarisation
} // namespace o2::aod

struct TaskPolarisationCharmHadrons {
  using CandDstarWSelFlag = soa::Join<aod::HfCandDstar, aod::HfSelDstarToD0Pi>;
  using CandLcToPKPiWSelFlag = soa::Join<aod::HfCand3Prong, aod::HfSelLc>;

  float massPi{0.f};
  float massProton{0.f};
  float massKaon{0.f};
  float massDstar{0.f};
  float massLc{0.f};
  float bkgRotationAngleStep{0.f};

  uint8_t nMassHypos{0u};

  Configurable<bool> selectionFlagDstarToD0Pi{"selectionFlagDstarToD0Pi", true, "Selection Flag for D* decay to D0 Pi"};
  Configurable<int> selectionFlagLcToPKPi{"selectionFlagLcToPKPi", 1, "Selection Flag for Lc decay to P K Pi"};

  ConfigurableAxis configThnAxisInvMass{"configThnAxisInvMass", {200, 0.139, 0.179}, "#it{M} (GeV/#it{c}^{2})"};
  ConfigurableAxis configThnAxisPt{"configThnAxisPt", {100, 0., 100.}, "#it{p}_{T} (GeV/#it{c})"};
  ConfigurableAxis configThnAxisPz{"configThnAxisPz", {100, -50., 50.}, "#it{p}_{z} (GeV/#it{c})"};
  ConfigurableAxis configThnAxisY{"configThnAxisY", {20, -1., 1.}, "#it{y}"};
  ConfigurableAxis configThnAxisCosThetaStarHelicity{"configThnAxisCosThetaStarHelicity", {20, -1., 1.}, "cos(#vartheta_{helicity})"};
  ConfigurableAxis configThnAxisCosThetaStarProduction{"configThnAxisCosThetaStarProduction", {20, -1., 1.}, "cos(#vartheta_{production})"};
  ConfigurableAxis configThnAxisCosThetaStarRandom{"configThnAxisCosThetaStarRandom", {20, -1., 1.}, "cos(#vartheta_{random})"};
  ConfigurableAxis configThnAxisCosThetaStarBeam{"configThnAxisCosThetaStarBeam", {20, -1., 1.}, "cos(#vartheta_{beam})"};
  ConfigurableAxis configThnAxisMlBkg{"configThnAxisMlBkg", {100, 0., 1.}, "ML bkg"};
  // ConfigurableAxis configThnAxisMlPrompt{"configThnAxisMlPrompt", {100, 0., 1.}, "ML prompt"};
  ConfigurableAxis configThnAxisMlNonPrompt{"configThnAxisMlNonPrompt", {100, 0., 1.}, "ML non-prompt"};
  // ConfigurableAxis configThnAxisCent{"configThnAxisCent", {102, -1., 101.}, "centrality (%)"};
  ConfigurableAxis configThnAxisIsRotatedCandidate{"configThnAxisIsRotatedCandidate", {2, -0.5, 1.5}, "0: standard candidate, 1: rotated candidate"};

  /// activate rotational background
  Configurable<int> nBkgRotations{"nBkgRotations", 0, "Number of rotated copies (background) per each original candidate"};

  /// output THnSparses
  Configurable<bool> activateTHnSparseCosThStarHelicity{"activateTHnSparseCosThStarHelicity", true, "Activate the THnSparse with cosThStar w.r.t. helicity axis"};
  Configurable<bool> activateTHnSparseCosThStarProduction{"activateTHnSparseCosThStarProduction", true, "Activate the THnSparse with cosThStar w.r.t. production axis"};
  Configurable<bool> activateTHnSparseCosThStarBeam{"activateTHnSparseCosThStarBeam", true, "Activate the THnSparse with cosThStar w.r.t. beam axis"};
  Configurable<bool> activateTHnSparseCosThStarRandom{"activateTHnSparseCosThStarRandom", true, "Activate the THnSparse with cosThStar w.r.t. random axis"};

  Filter filterSelectDstarCandidates = aod::hf_sel_candidate_dstar::isSelDstarToD0Pi == selectionFlagDstarToD0Pi;
  Filter filterSelectLcToPKPiCandidates = (aod::hf_sel_candidate_lc::isSelLcToPKPi >= selectionFlagLcToPKPi) || (aod::hf_sel_candidate_lc::isSelLcToPiKP >= selectionFlagLcToPKPi);

  HfHelper hfHelper;
  HistogramRegistry registry{"registry", {}};

  void init(InitContext&)
  {
    /// check process functions
    std::array<int, 4> processes = {doprocessDstar, doprocessDstarWithMl, doprocessLcToPKPi, doprocessLcToPKPiWithMl};
    const int nProcesses = std::accumulate(processes.begin(), processes.end(), 0);
    if (nProcesses > 1) {
      LOGP(fatal, "Only one process function should be enabled at a time, please check your configuration");
    } else if (nProcesses == 0) {
      LOGP(fatal, "No process function enabled");
    }

    /// check output THnSparses
    std::array<int, 4> sparses = {activateTHnSparseCosThStarHelicity, activateTHnSparseCosThStarProduction, activateTHnSparseCosThStarBeam, activateTHnSparseCosThStarRandom};
    if (std::accumulate(sparses.begin(), sparses.end(), 0) == 0) {
      LOGP(fatal, "No output THnSparses enabled");
    } else {
      if (activateTHnSparseCosThStarHelicity) {
        LOGP(info, "THnSparse with cosThStar w.r.t. helicity axis active.");
      }
      if (activateTHnSparseCosThStarProduction) {
        LOGP(info, "THnSparse with cosThStar w.r.t. production axis active.");
      }
      if (activateTHnSparseCosThStarBeam) {
        LOGP(info, "THnSparse with cosThStar w.r.t. beam axis active.");
      }
      if (activateTHnSparseCosThStarRandom) {
        LOGP(info, "THnSparse with cosThStar w.r.t. random axis active.");
      }
    }

    massPi = o2::constants::physics::MassPiPlus;
    massProton = o2::constants::physics::MassProton;
    massKaon = o2::constants::physics::MassKaonCharged;
    massDstar = o2::constants::physics::MassDStar;
    massLc = o2::constants::physics::MassLambdaCPlus;
    bkgRotationAngleStep = constants::math::TwoPI / (nBkgRotations + 1); // nBkgRotations==0: 2π (no rotation); nBkgRotations==1: π; nBkgRotations==2: 2π/3, 4π/3; ...

    const AxisSpec thnAxisInvMass{configThnAxisInvMass, "#it{M} (GeV/#it{c}^{2})"};
    const AxisSpec thnAxisPt{configThnAxisPt, "#it{p}_{T} (GeV/#it{c})"};
    const AxisSpec thnAxisPz{configThnAxisPz, "#it{p}_{z} (GeV/#it{c})"};
    const AxisSpec thnAxisY{configThnAxisY, "#it{y}"};
    const AxisSpec thnAxisCosThetaStarHelicity{configThnAxisCosThetaStarHelicity, "cos(#vartheta_{helicity})"};
    const AxisSpec thnAxisCosThetaStarProduction{configThnAxisCosThetaStarProduction, "cos(#vartheta_{production})"};
    const AxisSpec thnAxisCosThetaStarRandom{configThnAxisCosThetaStarRandom, "cos(#vartheta_{random})"};
    const AxisSpec thnAxisCosThetaStarBeam{configThnAxisCosThetaStarBeam, "cos(#vartheta_{beam})"};
    const AxisSpec thnAxisMlBkg{configThnAxisMlBkg, "ML bkg"};
    const AxisSpec thnAxisMlNonPrompt{configThnAxisMlNonPrompt, "ML non-prompt"};
    const AxisSpec thnAxisIsRotatedCandidate{configThnAxisIsRotatedCandidate, "0: standard candidate, 1: rotated candidate"};

    if (doprocessDstarWithMl) {
      /// analysis for D*+ meson with ML, w/o rot. background axis
      if (activateTHnSparseCosThStarHelicity) {
        registry.add("hSparseCharmPolarisationHelicity", "THn for polarisation studies with cosThStar w.r.t. helicity axis and BDT scores", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarHelicity, thnAxisMlBkg, thnAxisMlNonPrompt});
      }
      if (activateTHnSparseCosThStarProduction) {
        registry.add("hSparseCharmPolarisationProduction", "THn for polarisation studies with cosThStar w.r.t. production axis and BDT scores", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarProduction, thnAxisMlBkg, thnAxisMlNonPrompt});
      }
      if (activateTHnSparseCosThStarBeam) {
        registry.add("hSparseCharmPolarisationBeam", "THn for polarisation studies with cosThStar w.r.t. beam axis and BDT scores", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarBeam, thnAxisMlBkg, thnAxisMlNonPrompt});
      }
      if (activateTHnSparseCosThStarRandom) {
        registry.add("hSparseCharmPolarisationRandom", "THn for polarisation studies with cosThStar w.r.t. random axis and BDT scores", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarRandom, thnAxisMlBkg, thnAxisMlNonPrompt});
      }
    } else if (doprocessLcToPKPiWithMl) {
      /// analysis for Lc+ baryon with ML, w/ rot. background axis
      if (activateTHnSparseCosThStarHelicity) {
        registry.add("hSparseCharmPolarisationHelicity", "THn for polarisation studies with cosThStar w.r.t. helicity axis and BDT scores", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarHelicity, thnAxisMlBkg, thnAxisMlNonPrompt, thnAxisIsRotatedCandidate});
      }
      if (activateTHnSparseCosThStarProduction) {
        registry.add("hSparseCharmPolarisationProduction", "THn for polarisation studies with cosThStar w.r.t. production axis and BDT scores", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarProduction, thnAxisMlBkg, thnAxisMlNonPrompt, thnAxisIsRotatedCandidate});
      }
      if (activateTHnSparseCosThStarBeam) {
        registry.add("hSparseCharmPolarisationBeam", "THn for polarisation studies with cosThStar w.r.t. beam axis and BDT scores", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarBeam, thnAxisMlBkg, thnAxisMlNonPrompt, thnAxisIsRotatedCandidate});
      }
      if (activateTHnSparseCosThStarRandom) {
        registry.add("hSparseCharmPolarisationRandom", "THn for polarisation studies with cosThStar w.r.t. random axis and BDT scores", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarRandom, thnAxisMlBkg, thnAxisMlNonPrompt, thnAxisIsRotatedCandidate});
      }
    } else if (doprocessDstar) {
      /// analysis for D*+ meson, rot. background axis
      if (activateTHnSparseCosThStarHelicity) {
        registry.add("hSparseCharmPolarisationHelicity", "THn for polarisation studies with cosThStar w.r.t. helicity axis", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarHelicity});
      }
      if (activateTHnSparseCosThStarProduction) {
        registry.add("hSparseCharmPolarisationProduction", "THn for polarisation studies with cosThStar w.r.t. production axis", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarProduction});
      }
      if (activateTHnSparseCosThStarBeam) {
        registry.add("hSparseCharmPolarisationBeam", "THn for polarisation studies with cosThStar w.r.t. beam axis", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarBeam});
      }
      if (activateTHnSparseCosThStarRandom) {
        registry.add("hSparseCharmPolarisationRandom", "THn for polarisation studies with cosThStar w.r.t. random axis", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarRandom});
      }
    } else if (doprocessLcToPKPi) {
      /// analysis for Lc+ baryon, rot. background axis
      if (activateTHnSparseCosThStarHelicity) {
        registry.add("hSparseCharmPolarisationHelicity", "THn for polarisation studies with cosThStar w.r.t. helicity axis", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarHelicity, thnAxisIsRotatedCandidate});
      }
      if (activateTHnSparseCosThStarProduction) {
        registry.add("hSparseCharmPolarisationProduction", "THn for polarisation studies with cosThStar w.r.t. production axis", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarProduction, thnAxisIsRotatedCandidate});
      }
      if (activateTHnSparseCosThStarBeam) {
        registry.add("hSparseCharmPolarisationBeam", "THn for polarisation studies with cosThStar w.r.t. beam axis", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarBeam, thnAxisIsRotatedCandidate});
      }
      if (activateTHnSparseCosThStarRandom) {
        registry.add("hSparseCharmPolarisationRandom", "THn for polarisation studies with cosThStar w.r.t. random axis", HistType::kTHnSparseF, {thnAxisInvMass, thnAxisPt, thnAxisPz, thnAxisY, thnAxisCosThetaStarRandom, thnAxisIsRotatedCandidate});
      }
    }

    // inv. mass hypothesis to loop over
    // e.g.: Lc->pKpi has the ambiguity pKpi vs. piKp
    if (doprocessLcToPKPi || doprocessLcToPKPiWithMl) {
      nMassHypos = charm_polarisation::MassHyposLcToPKPi::NMassHypoLcToPKPi;
    } else {
      // D*, Lc->pK0s
      nMassHypos = 1;
    }

  }; // end init

  /// \param candidates are the selected candidates
  template <uint8_t channel, bool withMl, typename Cand>
  void runPolarisationAnalysis(Cand const& candidate, int bkgRotationId = 0)
  {

    // loop over mass hypotheses
    for (uint8_t iMass = 0u; iMass < nMassHypos; iMass++) {

      // variable definition
      float pxDau{-1000.}, pyDau{-1000.}, pzDau{-1000.};
      float pxCharmHad{-1000.}, pyCharmHad{-1000.}, pzCharmHad{-1000.};
      float massDau{0.}, invMassCharmHad{0.}, invMassCharmHadForSparse{0.};
      float rapidity{-999.};
      std::array<float, 3> outputMl{-1., -1., -1.};
      int isRotatedCandidate = 0; // currently meaningful only for Lc->pKpi

      if constexpr (channel == charm_polarisation::DecayChannel::DstarToDzeroPi) {
        // Dstar analysis
        // polarization measured from the soft-pion daughter (*)

        pxDau = candidate.pxSoftPi();
        pyDau = candidate.pySoftPi();
        pzDau = candidate.pzSoftPi();
        pxCharmHad = candidate.pxDstar();
        pyCharmHad = candidate.pyDstar();
        pzCharmHad = candidate.pzDstar();
        massDau = massPi; // (*)
        invMassCharmHad = (candidate.signSoftPi() > 0) ? candidate.invMassDstar() : candidate.invMassAntiDstar();
        invMassCharmHadForSparse = (candidate.signSoftPi() > 0) ? (invMassCharmHad - candidate.invMassD0()) : (invMassCharmHad - candidate.invMassD0Bar()); // different for D*
        rapidity = candidate.y(massDstar);
        if constexpr (withMl) {
          outputMl[0] = -1.; // not yet implemented in the selector
          outputMl[1] = -1.; // not yet implemented in the selector
          outputMl[2] = -1.; // not yet implemented in the selector
        }
      } else if constexpr (channel == charm_polarisation::DecayChannel::LcToPKPi) {
        // Lc->pKpi analysis
        // polarization measured from the proton daughter (*)

        /// mass-hypothesis-independent variables
        /// daughters momenta
        const float bkgRotAngle = bkgRotationAngleStep * bkgRotationId;
        std::array<float, 3> threeVecLcProng0{candidate.pxProng0(), candidate.pyProng0(), candidate.pzProng0()};
        std::array<float, 3> threeVecLcRotatedProng1{candidate.pxProng1() * std::cos(bkgRotAngle) - candidate.pyProng1() * std::sin(bkgRotAngle), candidate.pxProng1() * std::sin(bkgRotAngle) + candidate.pyProng1() * std::cos(bkgRotAngle), candidate.pzProng1()};
        std::array<float, 3> threeVecLcProng2{candidate.pxProng2(), candidate.pyProng2(), candidate.pzProng2()};
        if (bkgRotationId > 0) {
          /// rotational background - pt of the kaon track rotated
          /// update candidate momentum
          isRotatedCandidate = 1;
          pxCharmHad = threeVecLcProng0[0] + threeVecLcRotatedProng1[0] + threeVecLcProng2[0];
          pyCharmHad = threeVecLcProng0[1] + threeVecLcRotatedProng1[1] + threeVecLcProng2[1];
          pzCharmHad = threeVecLcProng0[2] + threeVecLcRotatedProng1[2] + threeVecLcProng2[2];
        } else {
          /// original candidate (kaon track not rotated)
          isRotatedCandidate = 0;
          pxCharmHad = candidate.px();
          pyCharmHad = candidate.py();
          pzCharmHad = candidate.pz();
        }
        massDau = massProton; // (*)
        rapidity = RecoDecay::y(std::array{pxCharmHad, pyCharmHad, pzCharmHad}, massLc);

        /// mass-hypothesis-dependent variables
        if (iMass == charm_polarisation::MassHyposLcToPKPi::PKPi && candidate.isSelLcToPKPi() >= selectionFlagLcToPKPi) {
          // reconstructed as pKpi
          pxDau = candidate.pxProng0();
          pyDau = candidate.pyProng0();
          pzDau = candidate.pzProng0();
          if (bkgRotationId) {
            /// rotational background - pt of the kaon track rotated
            invMassCharmHad = RecoDecay::m(std::array{threeVecLcProng0, threeVecLcRotatedProng1, threeVecLcProng2}, std::array{massProton, massKaon, massPi});
            invMassCharmHadForSparse = invMassCharmHad;
          } else {
            /// original candidate (kaon track not rotated)
            invMassCharmHad = hfHelper.invMassLcToPKPi(candidate);
            invMassCharmHadForSparse = hfHelper.invMassLcToPKPi(candidate);
          }
          if constexpr (withMl) {
            if (candidate.mlProbLcToPKPi().size() == 3) {
              // protect from empty vectors
              // the BDT output score might be empty if no preselections were enabled (selectionFlag null)
              // !!! NB: each rotated candidates inherits the BDT scores of the original candidate, even if the candidate pt changed after the rotation of the kaon-track pt !!!
              outputMl[0] = candidate.mlProbLcToPKPi()[0];
              outputMl[1] = candidate.mlProbLcToPKPi()[1];
              outputMl[2] = candidate.mlProbLcToPKPi()[2];
            }
          }
        } else if (iMass == charm_polarisation::MassHyposLcToPKPi::PiKP && candidate.isSelLcToPiKP() >= selectionFlagLcToPKPi) {
          // reconstructed as piKp
          pxDau = candidate.pxProng2();
          pyDau = candidate.pyProng2();
          pzDau = candidate.pzProng2();
          if (bkgRotationId) {
            /// rotational background - pt of the kaon track rotated
            invMassCharmHad = RecoDecay::m(std::array{threeVecLcProng0, threeVecLcRotatedProng1, threeVecLcProng2}, std::array{massPi, massKaon, massProton});
            invMassCharmHadForSparse = invMassCharmHad;
          } else {
            /// original candidate (kaon track not rotated)
            invMassCharmHad = hfHelper.invMassLcToPiKP(candidate);
            invMassCharmHadForSparse = hfHelper.invMassLcToPiKP(candidate);
          }
          if constexpr (withMl) {
            if (candidate.mlProbLcToPiKP().size() == 3) {
              // protect from empty vectors
              // the BDT output score might be empty if no preselections were enabled (selectionFlag null)
              // !!! NB: each rotated candidates inherits the BDT scores of the original candidate, even if the candidate pt changed after the rotation of the kaon-track pt !!!
              outputMl[0] = candidate.mlProbLcToPiKP()[0];
              outputMl[1] = candidate.mlProbLcToPiKP()[1];
              outputMl[2] = candidate.mlProbLcToPiKP()[2];
            }
          }
        } else {
          // NB: no need to check cases in which candidate.isSelLcToPKPi() and candidate.isSelLcToPiKP() are both false, because they are rejected already by the Filter
          // ... but we need to put this protections here!
          // Otherwise, a candidate selected as pKpi only has invMassCharmHad==0 when iMass == charm_polarisation::MassHyposLcToPKPi::PiKP and viceversa
          continue;
        }

      } // Lc->pKpi

      float phiRandom = gRandom->Uniform(0., constants::math::TwoPI);
      float thetaRandom = gRandom->Uniform(0., constants::math::PI);
      ROOT::Math::PxPyPzMVector fourVecDau = ROOT::Math::PxPyPzMVector(pxDau, pyDau, pzDau, massDau);
      ROOT::Math::PxPyPzMVector fourVecMother = ROOT::Math::PxPyPzMVector(pxCharmHad, pyCharmHad, pzCharmHad, invMassCharmHad);
      ROOT::Math::Boost boost{fourVecMother.BoostToCM()};
      ROOT::Math::PxPyPzMVector fourVecDauCM = boost(fourVecDau);
      ROOT::Math::XYZVector threeVecDauCM = fourVecDauCM.Vect();

      ROOT::Math::XYZVector randomVec = ROOT::Math::XYZVector(std::sin(thetaRandom) * std::cos(phiRandom), std::sin(thetaRandom) * std::sin(phiRandom), std::cos(thetaRandom));
      ROOT::Math::XYZVector beamVec = ROOT::Math::XYZVector(0., 0., 1.);
      ROOT::Math::XYZVector helicityVec = fourVecMother.Vect();
      ROOT::Math::XYZVector normalVec = ROOT::Math::XYZVector(pyCharmHad, -pxCharmHad, 0.);

      float cosThetaStarHelicity = helicityVec.Dot(threeVecDauCM) / std::sqrt(threeVecDauCM.Mag2()) / std::sqrt(helicityVec.Mag2());
      float cosThetaStarProduction = normalVec.Dot(threeVecDauCM) / std::sqrt(threeVecDauCM.Mag2()) / std::sqrt(normalVec.Mag2());
      float cosThetaStarBeam = beamVec.Dot(threeVecDauCM) / std::sqrt(threeVecDauCM.Mag2());
      float cosThetaStarRandom = randomVec.Dot(threeVecDauCM) / std::sqrt(threeVecDauCM.Mag2());

      const float candidatePt = std::sqrt(pxCharmHad * pxCharmHad + pyCharmHad * pyCharmHad); // this definition is valid for both rotated and original candidates
      if constexpr (withMl) {
        if (activateTHnSparseCosThStarHelicity) {
          if (channel == charm_polarisation::DecayChannel::DstarToDzeroPi) {
            // D*+
            registry.fill(HIST("hSparseCharmPolarisationHelicity"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarHelicity, outputMl[0], /*outputMl[1],*/ outputMl[2]);
          } else if (channel == charm_polarisation::DecayChannel::LcToPKPi) {
            // Lc+
            registry.fill(HIST("hSparseCharmPolarisationHelicity"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarHelicity, outputMl[0], /*outputMl[1],*/ outputMl[2], isRotatedCandidate);
          }
        }
        if (activateTHnSparseCosThStarProduction) {
          if (channel == charm_polarisation::DecayChannel::DstarToDzeroPi) {
            // D*+
            registry.fill(HIST("hSparseCharmPolarisationProduction"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarProduction, outputMl[0], /*outputMl[1],*/ outputMl[2]);
          } else if (channel == charm_polarisation::DecayChannel::LcToPKPi) {
            // Lc+
            registry.fill(HIST("hSparseCharmPolarisationProduction"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarProduction, outputMl[0], /*outputMl[1],*/ outputMl[2], isRotatedCandidate);
          }
        }
        if (activateTHnSparseCosThStarBeam) {
          if (channel == charm_polarisation::DecayChannel::DstarToDzeroPi) {
            // D*+
            registry.fill(HIST("hSparseCharmPolarisationBeam"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarBeam, outputMl[0], /*outputMl[1],*/ outputMl[2]);
          } else if (channel == charm_polarisation::DecayChannel::LcToPKPi) {
            // Lc+
            registry.fill(HIST("hSparseCharmPolarisationBeam"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarBeam, outputMl[0], /*outputMl[1],*/ outputMl[2], isRotatedCandidate);
          }
        }
        if (activateTHnSparseCosThStarRandom) {
          if (channel == charm_polarisation::DecayChannel::DstarToDzeroPi) {
            // D*+
            registry.fill(HIST("hSparseCharmPolarisationRandom"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarRandom, outputMl[0], /*outputMl[1],*/ outputMl[2]);
          } else if (channel == charm_polarisation::DecayChannel::LcToPKPi) {
            // Lc+
            registry.fill(HIST("hSparseCharmPolarisationRandom"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarRandom, outputMl[0], /*outputMl[1],*/ outputMl[2], isRotatedCandidate);
          }
        }
      } else {
        if (activateTHnSparseCosThStarHelicity) {
          if (channel == charm_polarisation::DecayChannel::DstarToDzeroPi) {
            // D*+
            registry.fill(HIST("hSparseCharmPolarisationHelicity"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarHelicity);
          } else if (channel == charm_polarisation::DecayChannel::LcToPKPi) {
            // Lc+
            registry.fill(HIST("hSparseCharmPolarisationHelicity"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarHelicity, isRotatedCandidate);
          }
        }
        if (activateTHnSparseCosThStarProduction) {
          if (channel == charm_polarisation::DecayChannel::DstarToDzeroPi) {
            // D*+
            registry.fill(HIST("hSparseCharmPolarisationProduction"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarProduction);
          } else if (channel == charm_polarisation::DecayChannel::LcToPKPi) {
            // Lc+
            registry.fill(HIST("hSparseCharmPolarisationProduction"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarProduction, isRotatedCandidate);
          }
        }
        if (activateTHnSparseCosThStarBeam) {
          if (channel == charm_polarisation::DecayChannel::DstarToDzeroPi) {
            // D*+
            registry.fill(HIST("hSparseCharmPolarisationBeam"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarBeam);
          } else if (channel == charm_polarisation::DecayChannel::LcToPKPi) {
            // Lc+
            registry.fill(HIST("hSparseCharmPolarisationBeam"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarBeam, isRotatedCandidate);
          }
        }
        if (activateTHnSparseCosThStarRandom) {
          if (channel == charm_polarisation::DecayChannel::DstarToDzeroPi) {
            // D*+
            registry.fill(HIST("hSparseCharmPolarisationRandom"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarRandom);
          } else if (channel == charm_polarisation::DecayChannel::LcToPKPi) {
            // Lc+
            registry.fill(HIST("hSparseCharmPolarisationRandom"), invMassCharmHadForSparse, candidatePt, pzCharmHad, rapidity, cosThetaStarRandom, isRotatedCandidate);
          }
        }
      }
    } /// end loop over mass hypotheses
  }

  /////////////////////////
  //   Dstar analysis   ///
  /////////////////////////

  // Dstar with rectangular cuts
  void processDstar(soa::Filtered<CandDstarWSelFlag>::iterator const& dstarCandidate)
  {
    runPolarisationAnalysis<charm_polarisation::DecayChannel::DstarToDzeroPi, false>(dstarCandidate);
  }
  PROCESS_SWITCH(TaskPolarisationCharmHadrons, processDstar, "Process Dstar candidates without ML", true);

  // Dstar with ML cuts
  void processDstarWithMl(soa::Filtered<CandDstarWSelFlag>::iterator const&)
  {
    // DUMMY
  }
  PROCESS_SWITCH(TaskPolarisationCharmHadrons, processDstarWithMl, "Process Dstar candidates with ML (DUMMY)", false);

  ////////////////////////////
  //   Lc->pKpi analysis   ///
  ////////////////////////////

  // Lc->pKpi with rectangular cuts
  void processLcToPKPi(soa::Filtered<CandLcToPKPiWSelFlag>::iterator const& lcCandidate)
  {
    runPolarisationAnalysis<charm_polarisation::DecayChannel::LcToPKPi, false>(lcCandidate);

    /// rotational background
    for (int iRotation = 1; iRotation <= nBkgRotations; iRotation++) {
      runPolarisationAnalysis<charm_polarisation::DecayChannel::LcToPKPi, false>(lcCandidate, iRotation);
    }
  }
  PROCESS_SWITCH(TaskPolarisationCharmHadrons, processLcToPKPi, "Process Lc candidates without ML", false);

  // Lc->pKpi with ML cuts
  void processLcToPKPiWithMl(soa::Filtered<soa::Join<CandLcToPKPiWSelFlag, aod::HfMlLcToPKPi>>::iterator const& lcCandidate)
  {
    runPolarisationAnalysis<charm_polarisation::DecayChannel::LcToPKPi, true>(lcCandidate);

    /// rotational background
    for (int iRotation = 1; iRotation <= nBkgRotations; iRotation++) {
      runPolarisationAnalysis<charm_polarisation::DecayChannel::LcToPKPi, true>(lcCandidate, iRotation);
    }
  }
  PROCESS_SWITCH(TaskPolarisationCharmHadrons, processLcToPKPiWithMl, "Process Lc candidates with ML", false);
};

WorkflowSpec defineDataProcessing(ConfigContext const& cfgc)
{
  return WorkflowSpec{adaptAnalysisTask<TaskPolarisationCharmHadrons>(cfgc)};
}
