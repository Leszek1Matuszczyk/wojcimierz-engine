#include "scene/Component.h"
#include "scene/GameObject.h"

namespace wz {

Transform& Component::transform() const { return owner_->transform(); }
Scene& Component::scene() const { return owner_->scene(); }
bool Component::IsActiveAndEnabled() const { return enabled && owner_ && owner_->IsActiveInHierarchy(); }

} // namespace wz
