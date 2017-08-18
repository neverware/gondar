// Copyright 2017 Neverware
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "device_picker.h"

#include <stdexcept>

#include "device.h"
#include "gondar.h"
#include "log.h"

namespace gondar {

constexpr int kIdRole = Qt::UserRole + 1;

DevicePicker::DevicePicker() {
  layout_.addWidget(&list_view_);
  layout_.setContentsMargins(0, 0, 0, 0);
  setLayout(&layout_);

  list_view_.setEditTriggers(QAbstractItemView::NoEditTriggers);
  list_view_.setModel(&model_);
  list_view_.setStyleSheet("::item { height: 3em; }");

  connect(list_view_.selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &DevicePicker::onSelectionChanged);
}

bool DevicePicker::hasSelection() const {
  return list_view_.selectionModel()->hasSelection();
}

bool DevicePicker::isEmpty() const {
  return model_.rowCount() == 0;
}

DeviceGuy DevicePicker::selection() const {
  const auto selection = list_view_.selectionModel()->selection();
  const auto indices = selection.indexes();

  if (indices.isEmpty()) {
    throw std::runtime_error("no selection");
  } else if (indices.size() > 1) {
    throw std::runtime_error("multiple selections");
  }

  return deviceFromRow(indices[0].row());
}

void DevicePicker::refresh() {
  auto devices = GetDeviceList();
  sort(devices.begin(), devices.end());

  // Remove rows for any devices that are no longer present
  for (int row = 0; row < model_.rowCount(); row++) {
    try {
      const auto rowDevice = deviceFromRow(row);
      if (!std::binary_search(devices.begin(), devices.end(), rowDevice)) {
        LOG_INFO << "DevicePicker: removing " << rowDevice.toString();
        model_.removeRow(row);
        row--;
      }
    } catch (const std::exception& err) {
      LOG_ERROR << err.what();
    }
  }

  // Add any new devices to the end of the model (to keep order
  // stable)
  for (const auto& device : devices) {
    if (!containsDevice(device)) {
      LOG_INFO << "DevicePicker: adding " << device.toString();
      auto* item = new QStandardItem(QString::fromStdString(device.name));
      item->setData(device.device_num, kIdRole);
      model_.appendRow(item);
    }
  }

  // TODO(nicholasbishop): add a unit test for this
}

DeviceGuy DevicePicker::deviceFromRow(const int row) const {
  if (const auto item = model_.item(row)) {
    const auto variant = item->data(kIdRole);
    bool ok = false;
    const auto id = variant.toInt(&ok);
    if (ok) {
      return DeviceGuy(id, item->text().toStdString());
    }
  }

  throw std::runtime_error("invalid row");
}

bool DevicePicker::containsDevice(const DeviceGuy& device) const {
  for (int row = 0; row < model_.rowCount(); row++) {
    try {
      const auto rowDevice = deviceFromRow(row);
      if (rowDevice == device) {
        return true;
      }
    } catch (const std::exception& err) {
      LOG_ERROR << err.what();
    }
  }

  return false;
}

void DevicePicker::onSelectionChanged() {
  emit selectionChanged();
}

}  // namespace gondar