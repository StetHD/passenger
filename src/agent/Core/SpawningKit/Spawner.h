/*
 *  Phusion Passenger - https://www.phusionpassenger.com/
 *  Copyright (c) 2011-2017 Phusion Holding B.V.
 *
 *  "Passenger", "Phusion Passenger" and "Union Station" are registered
 *  trademarks of Phusion Holding B.V.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 */
#ifndef _PASSENGER_SPAWNING_KIT_SPAWNER_H_
#define _PASSENGER_SPAWNING_KIT_SPAWNER_H_

#include <boost/shared_ptr.hpp>
#include <Utils/SystemTime.h>
#include <Core/SpawningKit/Context.h>
#include <Core/SpawningKit/Result.h>

namespace Passenger {
namespace SpawningKit {

using namespace std;
using namespace boost;
using namespace oxt;


class Spawner {
protected:
	Context *context;

	void setConfigFromAppPoolOptions(Config *config, Json::Value &extraArgs,
		const AppPoolOptions &options)
	{
		config->appRoot = options.appRoot;
		config->logLevel = options.logLevel;
		config->genericApp;
		config->startsUsingWrapper;
		config->findFreePort;
		config->loadShellEnvvars = options.loadShellEnvvars;
		config->analyticsSupport = options.analytics;
		config->startCommand;
		config->startupFile;
		config->appType = options.appType;
		config->appEnv = options.environment;
		config->baseURI = options.baseURI;
		config->environmentVariables = decodeEnvironmentVariables(
			options.environmentVariables);
		config->unionStationKey = options.unionStationKey;
		config->stickySessionId = options.stickySessionId;
		config->apiKey = options.apiKey;
		config->groupUuid = options.groupUuid;
		config->lveMinUid = options.lveMinUid;
		config->fileDescriptorUlimit = options.fileDescriptorUlimit;
		config->startTimeoutMsec = options.startTimeoutMsec;

		UserSwitchingInfo info = prepareUserSwitching(options);
		config->user = info.username;
		config->group = info.groupname;

		if (!options.appGroupName.empty()) {
			extraArgs["app_group_name"] = options.appGroupName.toString();
		}
		extraArgs["spawn_method"] = options.spawnMethod.toString();
		extraArgs["ust_router_address"] = options.ustRouterAddress.toString();
		extraArgs["ust_router_username"] = options.ustRouterUsername.toString();
		extraArgs["ust_router_password"] = options.ustRouterPassword.toString();

		/******************/
	}

public:
	/**
	 * Timestamp at which this Spawner was created. Microseconds resolution.
	 */
	const unsigned long long creationTime;

	Spawner(const Context *_context)
		: context(_context),
		  creationTime(SystemTime::getUsec())
		{ }

	virtual ~Spawner() { }

	virtual Result spawn(const AppPoolOptions &options) = 0;

	virtual bool cleanable() const {
		return false;
	}

	virtual void cleanup() {
		// Do nothing.
	}

	virtual unsigned long long lastUsed() const {
		return 0;
	}

	Context *getContext() const {
		return context;
	}
};
typedef boost::shared_ptr<Spawner> SpawnerPtr;


} // namespace SpawningKit
} // namespace Passenger

#endif /* _PASSENGER_SPAWNING_KIT_SPAWNER_H_ */
