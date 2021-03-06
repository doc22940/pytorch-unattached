#include "caffe2/c10_prototype/c10/cpu/CPUStorage.h"
#include "caffe2/c10_prototype/c10/guts/TensorImpl.h"
#include "caffe2/c10_prototype/c10/DimVector.h"
#include "caffe2/utils/Optional.h"
#include "caffe2/core/dispatch/Dispatcher.h"
#include "caffe2/core/dispatch/DeviceId.h"

#include <numeric>
#include <cmath>

namespace c10 { namespace cpu {

/**
 * Specialization of TensorImpl for CPU tensors.  Data layout is the same but we can make
 * extra assumptions about the types of some members.
 */
class CPUTensorImpl final : public guts::TensorImpl {
public:
  explicit CPUTensorImpl(caffe2::TypeMeta dtype)
  : TensorImpl(DeviceTypeId::CPU, LayoutId(0), {0}, {1}, std::make_shared<CPUStorageImpl>(dtype), 0)
  {};

  CPUStorage cpu_storage() {
    return std::static_pointer_cast<CPUStorageImpl>(storage_);
  }
};

}} // namespace c10::cpu
