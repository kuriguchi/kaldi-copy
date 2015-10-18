// ctc/cctc-tombstone-test.cc

// Copyright      2015   Johns Hopkins University (author: Daniel Povey)

// See ../../COPYING for clarification regarding multiple authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//  http://www.apache.org/licenses/LICENSE-2.0
//
// THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
// WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
// MERCHANTABLITY OR NON-INFRINGEMENT.
// See the Apache 2 License for the specific language governing permissions and
// limitations under the License.

#include "ctc/cctc-transition-model.h"
#include "ctc/cctc-tombstone.h"
#include "ctc/cctc-graph.h"
#include "ctc/language-model.h"
#include "ctc/cctc-supervision.h"
#include "ctc/cctc-training.h"
#include "ctc/cctc-test-utils.h"
#include "fstext/fstext-lib.h"
#include "cudamatrix/cu-device.h"

// This test program tests things declared in ctc-supervision.h and
// cctc-training.h

namespace kaldi {
namespace ctc {

void TestCctcTombstone(const CctcTransitionModel &trans_model) {
  CuMatrix<BaseFloat> weights;
  trans_model.ComputeWeights(&weights);
  CctcHmm hmm(trans_model);

  int32 num_sequences = RandInt(1, 5),
      num_time_steps = RandInt(10, 20);
  CuMatrix<BaseFloat> nnet_output(num_sequences * num_time_steps,
                                  trans_model.NumOutputIndexes());
  bool zero_output = (RandInt(0, 3) == 0);
  if (!zero_output)
    nnet_output.SetRandn();
  CuMatrix<BaseFloat> exp_nnet_output(nnet_output);
  exp_nnet_output.ApplyExp();

  CuMatrix<BaseFloat> denominators(nnet_output.NumRows(),
                                   trans_model.NumHistoryStates());

  denominators.AddMatMat(1.0, exp_nnet_output, kNoTrans, weights, kTrans, 0.0);

  CctcNegativeComputation negative_computation(trans_model, weights, hmm,
                                               exp_nnet_output,
                                               denominators, num_sequences);

  BaseFloat forward_prob = negative_computation.Forward(),
      per_frame = forward_prob / (num_sequences * num_time_steps);
  KALDI_LOG << "Forward prob is " << forward_prob
            << " = " << per_frame << " per frame.";
  if (zero_output)
    KALDI_ASSERT(ApproxEqual(BaseFloat(log(2.0 / 3.0)),
                             per_frame));

  CuMatrix<BaseFloat> denominators_deriv(denominators.NumRows(),
                                         denominators.NumCols(),
                                         kUndefined),
      nnet_output_deriv(nnet_output.NumRows(),
                        nnet_output.NumCols(),
                        kUndefined);
  negative_computation.Backward(&nnet_output_deriv,
                                &denominators_deriv);

}


void CctcTombstoneTest() {
  CctcTransitionModel trans_model;
  GenerateCctcTransitionModel(&trans_model);
  TestCctcTombstone(trans_model);
}


}  // namespace ctc
}  // namespace kaldi

int main() {
  using namespace kaldi;
  // will later change this to "< 2" and test with GPU.
  for (int32 loop = 0; loop < 1; loop++) {
#if HAVE_CUDA == 1
    if (loop == 0)
      CuDevice::Instantiate().SelectGpuId("no");
    else
      CuDevice::Instantiate().SelectGpuId("yes");
#endif
    for (int32 i = 0; i < 10; i++) {
      kaldi::ctc::CctcTombstoneTest();
    }
  }
}