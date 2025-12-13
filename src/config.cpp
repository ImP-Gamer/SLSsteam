#include "config.hpp"

#include "config_default.hpp"
#include "filewatcher.hpp"
#include "log.hpp"
#include "yaml-cpp/yaml.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>


std::string CConfig::getDir()
{
	char pathBuf[255];
	const char* configDir = getenv("XDG_CONFIG_HOME"); //Most users should have this set iirc
	if (configDir != NULL)
	{
		sprintf(pathBuf, "%s/SLSsteam", configDir);
	} else
	{
		const char* home = getenv("HOME");
		sprintf(pathBuf, "%s/.config/SLSsteam", home);
	}

	return std::string(pathBuf);
}

std::string CConfig::getPath()
{
	return getDir().append("/config.yaml");
}

bool CConfig::createFile()
{
	std::string path = getPath();
	if (!std::filesystem::exists(path))
	{
		std::string dir = getDir();
		if (!std::filesystem::exists(dir))
		{
			if (!std::filesystem::create_directory(dir))
			{
				g_pLog->notify("Unable to create config directory at %s!\n", dir.c_str());
				return false;
			}

			g_pLog->debug("Created config directory at %s\n", dir.c_str());
		}

		FILE* file = fopen(path.c_str(), "w");
		if (!file)
		{
			g_pLog->notify("Unable to create config at %s!\n", path.c_str());
			return false;
		}

		fputs(defaultConfig, file);
		fflush(file);
		fclose(file);
	}

	return true;
}

static void onFileChange()
{
	g_config.loadSettings();
}

bool CConfig::init()
{
	if(createFile())
	{
		watcher = new CFileWatcher(onFileChange);
		watcher->addFile(getPath().c_str());
		watcher->start();
	}

	loadSettings();
	return true;
}

CConfig::~CConfig()
{
	if (watcher)
	{
		delete watcher;
	}
}

bool CConfig::loadSettings()
{
	YAML::Node node;
	try
	{
		node = YAML::LoadFile(getPath());
	}
	catch (YAML::BadFile& bf)
	{
		g_pLog->notifyLong("Can not read config.yaml! %s\nUsing defaults", bf.msg.c_str());
		node = YAML::Node(); //Create empty node and let defaults kick in
	}
	catch (YAML::ParserException& pe)
	{
		g_pLog->notifyLong("Error parsing config.yaml! %s\nUsing defaults", pe.msg.c_str());
		node = YAML::Node(); //Create empty node and let defaults kick in
	}
	
	disableFamilyLock = getSetting<bool>(node, "DisableFamilyShareLock", true);
	useWhiteList = getSetting<bool>(node, "UseWhitelist", false);
	automaticFilter = getSetting<bool>(node, "AutoFilterList", true);
	playNotOwnedGames = getSetting<bool>(node, "PlayNotOwnedGames", false);
	safeMode = getSetting<bool>(node, "SafeMode", false);
	notifications = getSetting<bool>(node, "Notifications", true);
	warnHashMissmatch = getSetting<bool>(node, "WarnHashMissmatch", false);
	notifyInit = getSetting<bool>(node, "NotifyInit", true);
	extendedLogging = getSetting<bool>(node, "ExtendedLogging", false);
	logLevel = getSetting<unsigned int>(node, "LogLevel", 2);
	blockEncryptedAppTickets = getSetting<bool>(node, "BlockEncryptedAppTickets", false);

	//TODO: Create smart logging function to log them automatically via getSetting
	g_pLog->info("DisableFamilyShareLock: %i\n", disableFamilyLock);
	g_pLog->info("UseWhitelist: %i\n", useWhiteList);
	g_pLog->info("AutoFilterList: %i\n", automaticFilter);
	g_pLog->info("PlayNotOwnedGames: %i\n", playNotOwnedGames);
	g_pLog->info("SafeMode: %i\n", safeMode);
	g_pLog->info("Notifications: %i\n", notifications);
	g_pLog->info("WarnHashMissmatch: %i\n", warnHashMissmatch);
	g_pLog->info("NotifyInit: %i\n", notifyInit);
	g_pLog->info("ExtendedLogging: %i\n", extendedLogging);
	g_pLog->info("LogLevel: %i\n", logLevel);
	g_pLog->info("BlockEncryptedAppTickets: %i\n", blockEncryptedAppTickets);

	appIds = getList<uint32_t>(node, "AppIds");
	addedAppIds = getList<uint32_t>(node, "AdditionalApps");
	fakeOffline = getList<uint32_t>(node, "FakeOffline");

	const auto fakeAppIdsNode = node["FakeAppIds"];
	if (fakeAppIdsNode)
	{
		for (const auto& node : fakeAppIdsNode)
		{
			try
			{
				uint32_t k = node.first.as<uint32_t>();
				uint32_t v = node.second.as<uint32_t>();
				fakeAppIds[k] = v;

				g_pLog->info("Added %u : %u to FakeAppIds\n", k, v);
			}
			catch(...)
			{
				g_pLog->warn("Failed to parse FakeAppIds!");
				break;
			}
		}
	}
	else
	{
		g_pLog->warn("Missing FakeAppIds entry in config!");
	}

	//Do not warn for these (yet?)
	const auto idleStatusNode = node["IdleStatus"];
	if (idleStatusNode)
	{
		try
		{
			idleStatus = FakeGame_t
			{
				idleStatusNode["AppId"].as<uint32_t>(),
				idleStatusNode["Title"].as<std::string>()
			};

			g_pLog->debug("Idle status %s with AppId %u\n", idleStatus.title.c_str(), idleStatus.appId);
		}
		catch(...)
		{
			g_pLog->warn("Failed to parse IdleStatus!");
		}
	}
	const auto unownedStatusNode = node["UnownedStatus"];
	if (unownedStatusNode)
	{
		try
		{
			unownedStatus = FakeGame_t
			{
				unownedStatusNode["AppId"].as<uint32_t>(),
				unownedStatusNode["Title"].as<std::string>()
			};

			g_pLog->debug("Unowned status %s with AppId %u\n", unownedStatus.title.c_str(), unownedStatus.appId);
		}
		catch(...)
		{
			g_pLog->warn("Failed to parse UnownedStatus");
		}
	}

	const auto dlcDataNode = node["DlcData"];
	if(dlcDataNode)
	{
		for(auto& app : dlcDataNode)
		{
			try
			{
				const uint32_t parentId = app.first.as<uint32_t>();

				CDlcData data;
				data.parentId = parentId;
				g_pLog->info("Adding DlcData for %u\n", parentId);

				for(auto& dlc : app.second)
				{
					const uint32_t dlcId = dlc.first.as<uint32_t>();
					//There's more efficient types to store strings, but they mostly do not work
					const std::string dlcName = dlc.second.as<std::string>();

					data.dlcIds[dlcId] = dlcName;
					g_pLog->info("DlcId %u -> %s\n", dlcId, dlcName.c_str());
				}

				dlcData[parentId] = data;
			}
			catch(...)
			{
				g_pLog->notify("Failed to parse DlcData!");
				break;
			}
		}
	}
	else
	{
		g_pLog->notify("Missing DlcData entry in config!");
	}

	const auto denuvoGamesNode = node["DenuvoGames"];
	if (denuvoGamesNode)
	{
		for (auto& steamIdNode : denuvoGamesNode)
		{
			try
			{
				const uint32_t steamId = steamIdNode.first.as<uint32_t>();
				denuvoGames[steamId] = std::unordered_set<uint32_t>();

				for (auto& appIdNode : steamIdNode.second)
				{
					const uint32_t appId = appIdNode.as<uint32_t>();
					denuvoGames[steamId].emplace(appId);

					//Again, not loggin SteamId because of privacy
					g_pLog->info("Added DenuvoGame %u\n", appId);
				}
			}
			catch (...)
			{
				g_pLog->notify("Failed to parse DenuvoGames!");
			}
		}
	}
	else
	{
		g_pLog->notify("Missing DenuvoGames entry in config!");
	}

	return true;
}

bool CConfig::isAddedAppId(uint32_t appId)
{
	return addedAppIds.contains(appId);
}

bool CConfig::addAdditionalAppId(uint32_t appId)
{
	if (isAddedAppId(appId))
		return false;

	addedAppIds.emplace(appId);
	g_pLog->once("Force owned %u\n", appId); //once is unnessecary but just for consistency
	return true;
}

bool CConfig::shouldExcludeAppId(uint32_t appId)
{
	bool exclude = false;
	//Proper way would be with getAppType, but that seems broken so we need to do this instead
	constexpr uint32_t ONE_BILLION = 1E9; //Implicit cast from double to unsigned int, hopefully this does not break anything
	if (appId >= ONE_BILLION) //Higher and equal to 10^9 gets used by Steam Internally
	{
		exclude = true;
	}
	else
	{
		bool found = appIds.contains(appId);
		exclude = !isAddedAppId(appId) && ((useWhiteList && !found) || (!useWhiteList && found));
	}

	g_pLog->once("shouldExcludeAppId(%u) -> %i\n", appId, exclude);
	return exclude;
}

uint32_t CConfig::getDenuvoGameOwner(uint32_t appId)
{
	for(const auto& tpl : denuvoGames)
	{
		if (tpl.second.contains(appId))
		{
			//g_pLog->once("%u is DenuvoGame\n", appId);
			return tpl.first;
		}
	}

	return 0;
}

CConfig g_config = CConfig();
