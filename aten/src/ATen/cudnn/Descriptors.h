#pragma once

#include "Exceptions.h"

#include "cudnn-wrapper.h"
#include <ATen/ATen.h>
#include <ATen/Check.h>

#if CUDNN_VERSION < 7000

// Reverse engineered from cuDNN 6.
struct cudnnDropoutStruct {
  float dropout;
  int nstates;
  void * states;
};

#endif

namespace at { namespace native {

// TODO: Add constructors for all of the descriptors

inline int dataSize(cudnnDataType_t dataType)
{
  switch (dataType) {
    case CUDNN_DATA_HALF: return 2;
    case CUDNN_DATA_FLOAT: return 4;
    default: return 8;
  }
}

// The stride for a size-1 dimensions is not uniquely determined; in
// fact, it can be anything you want, because the fact that the
// tensor is size 1 at this dimension means that you will never actually
// try advancing your pointer by this stride.
//
// However, CuDNN has a much more stringent requirement on strides:
// if you are passing a contiguous input, it better be the case
// that the stride for dim i is the product of the sizes of dims
// i+1 to the end.  This stride is indeed uniquely determined.  This
// function modifies 'stride' in place so this invariant holds.
static inline void fixSizeOneDimStride(int dim, const int *size, int *stride) {
  int64_t z = 1;
  for(int d = dim-1; d >= 0; d--)
  {
    if (size[d] == 1) {
      stride[d] = z;
    } else {
      z *= size[d];
    }
  }
}

// TODO: Use unique_ptr on this?
struct TensorDescriptor
{
  cudnnTensorDescriptor_t desc;

  explicit TensorDescriptor() : desc(nullptr) {
    CUDNN_CHECK(cudnnCreateTensorDescriptor(&desc));
  };

  TensorDescriptor(const TensorDescriptor& ref) = delete;
  TensorDescriptor(TensorDescriptor&& ref) = delete;

  ~TensorDescriptor() {
    cudnnDestroyTensorDescriptor(desc);
  }

  // Note [CuDNN broadcast padding]
  // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // pad specifies the minimum dimensionality of the tensor descriptor
  // we produce (it doesn't have anything to do with, e.g., convolution
  // padding).  If 't' is lower-dimensional than 'pad', the remaining
  // dimensions (on the right) are padded with ones.  This doesn't
  // affect the underlying data layout.  This is particularly useful for
  // dealing with a pecularity of the CuDNN API, which is that broadcasting in CuDNN is
  // done in two steps: first, the client code is expected to pad out
  // (the dimensions) input tensors to be the same dimension as the
  // target broadcast, and then second, CuDNN takes of actually
  // broadcasting size 1 dimensions.
  void set(const at::Tensor &t, int64_t pad = 0);

  explicit TensorDescriptor(const at::Tensor &t, int64_t pad = 0) : desc(nullptr) {
    CUDNN_CHECK(cudnnCreateTensorDescriptor(&desc));
    set(t, pad);
  }

  void print();

private:
  void set(cudnnDataType_t dataType, int dim, int* size, int* stride) {
    fixSizeOneDimStride(dim, size, stride);
    CUDNN_CHECK(cudnnSetTensorNdDescriptor(desc, dataType, dim, size, stride));
  }
};

std::ostream& operator<<(std::ostream & out, const TensorDescriptor& d);

struct FilterDescriptor
{
  cudnnFilterDescriptor_t desc;

  explicit FilterDescriptor() : desc(nullptr) {
    CUDNN_CHECK(cudnnCreateFilterDescriptor(&desc));
  }
  FilterDescriptor(const FilterDescriptor&) = delete;
  FilterDescriptor(FilterDescriptor&& ref) = delete;

  ~FilterDescriptor() {
    cudnnDestroyFilterDescriptor(desc);
  }

  void set(const at::Tensor &t, int64_t pad = 0);

private:
  void set(cudnnDataType_t dataType, int dim, int* size) {
    CUDNN_CHECK(cudnnSetFilterNdDescriptor(desc, dataType, CUDNN_TENSOR_NCHW, dim, size));
  }
};

struct ConvolutionDescriptor
{
  cudnnConvolutionDescriptor_t desc;
  ConvolutionDescriptor() : desc(NULL) {
    CUDNN_CHECK(cudnnCreateConvolutionDescriptor(&desc));
  }
  ConvolutionDescriptor(const ConvolutionDescriptor&) = delete;
  ConvolutionDescriptor(ConvolutionDescriptor&& ref)
  {
    desc = ref.desc;
    ref.desc = NULL;
  }
  ~ConvolutionDescriptor() {
    cudnnDestroyConvolutionDescriptor(desc);
  }
  void set(cudnnDataType_t dataType, int dim, int* pad, int* stride, int * upscale /* aka dilation */, int groups) {
    cudnnDataType_t mathType = dataType;
    if (dataType == CUDNN_DATA_HALF) mathType = CUDNN_DATA_FLOAT;
    CUDNN_CHECK(cudnnSetConvolutionNdDescriptor(desc, dim, pad, stride, upscale,
                                          CUDNN_CROSS_CORRELATION, mathType));
#if CUDNN_VERSION >= 7000
    CUDNN_CHECK(cudnnSetConvolutionGroupCount(desc, groups));
    CUDNN_CHECK(cudnnSetConvolutionMathType(desc, CUDNN_DEFAULT_MATH));
    if(dataType == CUDNN_DATA_HALF)
      CUDNN_CHECK(cudnnSetConvolutionMathType(desc, CUDNN_TENSOR_OP_MATH));
#endif
  }
};

struct SpatialTransformerDescriptor
{
  cudnnSpatialTransformerDescriptor_t desc;
  SpatialTransformerDescriptor() : desc(NULL) {
    CUDNN_CHECK(cudnnCreateSpatialTransformerDescriptor(&desc));
  }
  SpatialTransformerDescriptor(const SpatialTransformerDescriptor&) = delete;
  SpatialTransformerDescriptor(SpatialTransformerDescriptor&& ref)
  {
    desc = ref.desc;
    ref.desc = NULL;
  }
  ~SpatialTransformerDescriptor() {
    cudnnDestroySpatialTransformerDescriptor(desc);
  }
  void set(cudnnDataType_t dataType, int dim, int* size) {
    CUDNN_CHECK(cudnnSetSpatialTransformerNdDescriptor(desc, CUDNN_SAMPLER_BILINEAR, dataType, dim, size));
  }
};

#if CUDNN_VERSION < 7000

inline cudnnStatus_t cudnnRestoreDropoutDescriptor(
    cudnnDropoutDescriptor_t dropoutDesc,
    cudnnHandle_t handle,
    float dropout,
    void *states,
    size_t stateSizeInBytes,
    unsigned long long seed) {
  dropoutDesc->dropout = dropout;
  dropoutDesc->nstates = stateSizeInBytes;
  dropoutDesc->states = states;
  return CUDNN_STATUS_SUCCESS;
}

#endif // CUDNN_VERSION

struct DropoutDescriptor
{
  cudnnDropoutDescriptor_t desc;
  at::Tensor state;
  DropoutDescriptor() : desc(nullptr) {
    CUDNN_CHECK(cudnnCreateDropoutDescriptor(&desc));
  }
  DropoutDescriptor(const DropoutDescriptor&) = delete;
  ~DropoutDescriptor() {
    cudnnDestroyDropoutDescriptor(desc);
  }
  // This is expensive, avoid calling me!
  void expensiveSet(cudnnHandle_t handle, float dropout, long long int seed) {
    void *state_ptr = nullptr;
    size_t state_size = 0;
    if (dropout > 0) {
      size_t dropout_state_size;
      CUDNN_CHECK(cudnnDropoutGetStatesSize(handle, &dropout_state_size));
      state = at::CUDA(kByte).tensor({static_cast<int64_t>(dropout_state_size)});
      state_ptr = state.data_ptr();
      state_size = state.size(0);
    }
    CUDNN_CHECK(cudnnSetDropoutDescriptor(desc, handle, dropout, state_ptr, state_size, seed));
  }
  // I'm cheap! Call me!
  void set(cudnnHandle_t handle, float dropout, at::Tensor state_, long long int seed) {
    void *state_ptr = nullptr;
    size_t state_size = 0;
    if (dropout > 0) {
      state = state_;
      state_ptr = state.data_ptr();
      state_size = state.size(0);
    }
    CUDNN_CHECK(cudnnRestoreDropoutDescriptor(desc, handle, dropout, state_ptr, state_size, seed));
  }
};

/*
struct DropoutDescriptor
{
  cudnnDropoutDescriptor_t desc;
  cudnnHandle_t handle;
  at::Tensor state;
  float dropout;
  DropoutDescriptor() : desc(nullptr) {
    CUDNN_CHECK(cudnnCreateDropoutDescriptor(&desc));
  }
  DropoutDescriptor(const DropoutDescriptor&) = delete;
  ~DropoutDescriptor() {
    cudnnDestroyDropoutDescriptor(desc);
  }
  void set(cudnnHandle_t handle, at::Tensor state_) {
  }
  void set(cudnnHandle_t handle, float dropout_, unsigned long long seed) {
    void *state_ptr;
    size_t state_size;
    if (!state.defined() && dropout_ > 0) {
      size_t dropout_state_size;
      CUDNN_CHECK(cudnnDropoutGetStatesSize(handle, &dropout_state_size));
      state = at::CUDA(kByte).tensor({static_cast<int64_t>(dropout_state_size)});
      state_ptr = state.data_ptr();
      state_size = state.size(0);
    } else {
      state_ptr = nullptr;
      state_size = 0;
    }
    CUDNN_CHECK(cudnnSetDropoutDescriptor(desc, handle, dropout_, state_ptr, state_size, seed));
    dropout = dropout_;
  }
};
*/

struct RNNDescriptor
{
  cudnnRNNDescriptor_t desc;
  DropoutDescriptor dropout_desc;
  RNNDescriptor() : desc(nullptr) {
    CUDNN_CHECK(cudnnCreateRNNDescriptor(&desc));
  }
  RNNDescriptor(const RNNDescriptor&) = delete;
  ~RNNDescriptor() {
    cudnnDestroyRNNDescriptor(desc);
  }
  // TODO: Borrows a reference to DropoutDescriptor
  void set(cudnnHandle_t handle, int hidden_size, int num_layers, const DropoutDescriptor& dropout_desc,
           cudnnRNNInputMode_t input_mode, cudnnDirectionMode_t bidirectional,
           cudnnRNNMode_t mode, cudnnDataType_t datatype) {
    CUDNN_CHECK(cudnnSetRNNDescriptor_v6(
          handle,
          desc,
          hidden_size,
          num_layers,
          dropout_desc.desc,
          input_mode,
          bidirectional,
          mode,
          CUDNN_RNN_ALGO_STANDARD,
          datatype));
#if CUDNN_VERSION >= 7000 && CUDA_VERSION >= 9000
    // TODO: This code should live as a utility somewhere in ATen.
    // Please don't copy paste me!
    int device;
    CUDA_CHECK(cudaGetDevice(&device));
    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, device));
    if (prop.major >= 7) {
      if (datatype == CUDNN_DATA_HALF) {
        cudnnSetRNNMatrixMathType(desc, CUDNN_TENSOR_OP_MATH);
      } else {
        // Technically, as the default it's not necessary to explicitly
        // set this.
        cudnnSetRNNMatrixMathType(desc, CUDNN_DEFAULT_MATH);
      }
    }
#endif
  }
};

union Constant
{
  float f;
  double d;
  Constant(cudnnDataType_t dataType, double value) {
    if (dataType == CUDNN_DATA_HALF || dataType == CUDNN_DATA_FLOAT) {
      f = (float) value;
    } else {
      d = value;
    }
  }
};

}}  // namespace
