/*
 *  Phusion Passenger - https://www.phusionpassenger.com/
 *  Copyright (c) 2014-2015 Phusion Holding B.V.
 *
 *  "Passenger", "Phusion Passenger" and "Union Station" are registered
 *  trademarks of Phusion Holding B.V.
 *
 *  See LICENSE file for license information.
 */
#ifndef _PASSENGER_APPLICATION_POOL2_BASIC_PROCESS_INFO_H_
#define _PASSENGER_APPLICATION_POOL2_BASIC_PROCESS_INFO_H_

#include <sys/types.h>
#include <cstring>

#include <jsoncpp/json.h>

#include <StaticString.h>
#include <Exceptions.h>
#include <Utils/JsonUtils.h>
#include <Core/ApplicationPool/BasicGroupInfo.h>

namespace Passenger {
namespace ApplicationPool2 {

using namespace std;


class Process;

/**
 * Contains a subset of the information in Process. This subset consists only
 * of information that is:
 *
 *  1. ...read-only and set during Process constructions.
 *  2. ...needed by Session.
 *
 * This class is contained inside `Process` as a const object. Because the
 * information is read-only, and because Process outlives all related Session
 * objects, Session can access it without grabbing the lock on Process.
 *
 * This class also serves to ensure that Session does not have a direct
 * dependency on Process.
 */
class BasicProcessInfo {
public:
	static const unsigned int GUPID_MAX_SIZE = 20;

	/** The Process that this BasicProcessInfo is contained in. */
	Process *process;

	/** The basic information of the Group that the associated Process is contained in. */
	const BasicGroupInfo *groupInfo;

	/**
	 * The operating system process ID.
	 */
	pid_t pid;

	/**
	 * An ID that uniquely identifies this Process in the Group, for
	 * use in implementing sticky sessions. Set by Group::attach().
	 */
	unsigned int stickySessionId;

	/**
	 * UUID for this process, randomly generated and extremely unlikely to ever
	 * appear again in this universe.
	 */
	char gupid[GUPID_MAX_SIZE];
	unsigned int gupidSize;


	BasicProcessInfo(Process *_process, const BasicGroupInfo *_groupInfo,
		const Json::Value &json)
		: process(_process),
		  groupInfo(_groupInfo),
		  pid(getJsonIntField(json, "pid"))
		  //stickySessionId(getJsonUintField(json, "sticky_session_id", 0))
	{
		StaticString gupid = getJsonStaticStringField(json, "gupid");
		assert(gupid.size() <= GUPID_MAX_SIZE);
		memcpy(this->gupid, gupid.data(), gupid.size());
		gupidSize = gupid.size();
	}
};


} // namespace ApplicationPool2
} // namespace Passenger

#endif /* _PASSENGER_APPLICATION_POOL2_BASIC_PROCESS_INFO_H_ */