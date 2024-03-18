/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <multipass/format.h>
#include <multipass/network_interface.h>
#include <multipass/yaml_node_utils.h>

namespace mp = multipass;

namespace
{
// remove this utility function once C++20 std::string::ends_with is available
bool ends_with(const std::string_view search_str, const std::string_view sub_str)
{
    if (sub_str.size() > search_str.size())
    {
        return false;
    }

    return std::equal(sub_str.crbegin(), sub_str.crend(), search_str.crbegin());
}

std::string toggle_instance_id(const std::string& original_instance_id)
{
    std::string result_instance_id{original_instance_id};
    const std::string tweak = "_e";

    // Check if the instance_id already ends with the tweak.
    if (ends_with(original_instance_id, tweak))
    {
        // Tweak found at the string end, remove it.
        result_instance_id.erase(result_instance_id.size() - tweak.size(), tweak.size());
    }
    else
    {
        // Tweak not found, append it.
        result_instance_id += tweak;
    }

    return result_instance_id;
}
} // namespace

std::string mp::utils::emit_yaml(const YAML::Node& node)
{
    YAML::Emitter emitter;
    emitter.SetIndent(2);
    emitter << node;
    if (!emitter.good())
        throw std::runtime_error{fmt::format("Failed to emit YAML: {}", emitter.GetLastError())};

    emitter << YAML::Newline;
    return emitter.c_str();
}

std::string mp::utils::emit_cloud_config(const YAML::Node& node)
{
    return fmt::format("#cloud-config\n{}", emit_yaml(node));
}

YAML::Node mp::utils::make_cloud_init_meta_config(const std::string& name, const std::string& file_content)
{
    YAML::Node meta_data = file_content.empty() ? YAML::Node{} : YAML::Load(file_content);

    meta_data["instance-id"] = name;
    meta_data["local-hostname"] = name;
    meta_data["cloud-name"] = "multipass";

    return meta_data;
}

YAML::Node mp::utils::make_cloud_init_meta_config_with_id_tweak(const std::string& file_content)
{
    YAML::Node meta_data = YAML::Load(file_content);

    meta_data["instance-id"] = YAML::Node{toggle_instance_id(meta_data["instance-id"].as<std::string>())};

    return meta_data;
}

YAML::Node mp::utils::make_cloud_init_network_config(const std::string& default_mac_addr,
                                                     const std::vector<mp::NetworkInterface>& extra_interfaces,
                                                     const std::string& file_content)
{
    YAML::Node network_data = file_content.empty() ? YAML::Node{} : YAML::Load(file_content);

    // Generate the cloud-init file only if there is at least one extra interface needing auto configuration.
    if (std::find_if(extra_interfaces.begin(), extra_interfaces.end(), [](const auto& iface) {
            return iface.auto_mode;
        }) != extra_interfaces.end())
    {
        network_data["version"] = "2";

        std::string name = "default";
        network_data["ethernets"][name]["match"]["macaddress"] = default_mac_addr;
        network_data["ethernets"][name]["dhcp4"] = true;

        for (size_t i = 0; i < extra_interfaces.size(); ++i)
        {
            if (extra_interfaces[i].auto_mode)
            {
                name = "extra" + std::to_string(i);
                network_data["ethernets"][name]["match"]["macaddress"] = extra_interfaces[i].mac_address;
                network_data["ethernets"][name]["dhcp4"] = true;
                // We make the default gateway associated with the first interface.
                network_data["ethernets"][name]["dhcp4-overrides"]["route-metric"] = 200;
                // Make the interface optional, which means that networkd will not wait for the device to be configured.
                network_data["ethernets"][name]["optional"] = true;
            }
        }
    }

    return network_data;
}
