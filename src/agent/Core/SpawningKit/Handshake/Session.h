/*
 *  Phusion Passenger - https://www.phusionpassenger.com/
 *  Copyright (c) 2016-2017 Phusion Holding B.V.
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
#ifndef _PASSENGER_SPAWNING_KIT_HANDSHAKE_SESSION_H_
#define _PASSENGER_SPAWNING_KIT_HANDSHAKE_SESSION_H_

#include <boost/scoped_ptr.hpp>
#include <string>

#include <Utils.h>
#include <Core/SpawningKit/Context.h>
#include <Core/SpawningKit/Config.h>
#include <Core/SpawningKit/Journey.h>
#include <Core/SpawningKit/Result.h>
#include <Core/SpawningKit/Handshake/WorkDir.h>


namespace Passenger {
namespace SpawningKit {

using namespace std;


struct HandshakeSession {
	Context *context;
	Config *config;

	boost::scoped_ptr<HandshakeWorkDir> workDir;
	string responseDir;
	Journey journey;
	Result result;

	uid_t uid;
	gid_t gid;
	string homedir;
	string shell;

	unsigned long long timeoutUsec;

	/**
	 * The port that the application is expected to start on. Only meaningful
	 * if `config->genericApp` is false.
	 */
	unsigned int expectedStartPort;

	HandshakeSession(JourneyType journeyType)
		: context(NULL),
		  config(NULL),
		  journey(journeyType),
		  uid(USER_NOT_GIVEN),
		  gid(GROUP_NOT_GIVEN),
		  timeoutUsec(0),
		  expectedStartPort(0)
		{ }
};


} // namespace SpawningKit
} // namespace Passenger

#endif /* _PASSENGER_SPAWNING_KIT_HANDSHAKE_SESSION_H_ */
