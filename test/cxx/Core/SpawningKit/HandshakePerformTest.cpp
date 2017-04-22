#include <TestSupport.h>
#include <Core/SpawningKit/Handshake/Prepare.h>
#include <Core/SpawningKit/Handshake/Perform.h>
#include <boost/bind.hpp>
#include <cstdio>
#include <Utils/IOUtils.h>

using namespace std;
using namespace Passenger;
using namespace Passenger::SpawningKit;

namespace tut {
	struct Core_SpawningKit_HandshakePerformTest {
		SpawningKit::Context::Schema schema;
		SpawningKit::Context context;
		SpawningKit::Config config;
		boost::shared_ptr<HandshakeSession> session;
		pid_t pid;
		HandshakePerform::DebugSupport *debugSupport;
		AtomicInt counter;
		FileDescriptor server;

		Core_SpawningKit_HandshakePerformTest()
			: context(schema),
			  pid(getpid()),
			  debugSupport(NULL)
		{
			context.resourceLocator = resourceLocator;
			context.integrationMode = "standalone";
			context.finalize();

			config.appRoot = "/tmp/myapp";
			config.startCommand = "echo hi";
			config.startupFile = "/tmp/myapp/app.py";
			config.appType = "wsgi";
			config.spawnMethod = "direct";
			config.user = getProcessUsername();
			config.group = getGroupName(getgid());
			config.internStrings();
		}

		~Core_SpawningKit_HandshakePerformTest() {
			setPrintAppOutputAsDebuggingMessages(false);
		}

		void init(JourneyType type) {
			vector<StaticString> errors;
			ensure("Config is valid", config.validate(errors));
			session = boost::make_shared<HandshakeSession>(context, config, type);

			session->journey.setStepInProgress(SPAWNING_KIT_PREPARATION);
			HandshakePrepare(*session).execute();

			session->journey.setStepInProgress(SPAWNING_KIT_HANDSHAKE_PERFORM);
			session->journey.setStepInProgress(SUBPROCESS_BEFORE_FIRST_EXEC);
		}

		void execute() {
			HandshakePerform performer(*session, pid);
			performer.debugSupport = debugSupport;
			performer.execute();
			counter++;
		}

		static Json::Value createGoodPropertiesJson() {
			Json::Value socket, doc;
			socket["address"] = "tcp://127.0.0.1:3000";
			socket["protocol"] = "http";
			socket["concurrency"] = 1;
			socket["accept_http_requests"] = true;
			doc["sockets"].append(socket);
			return doc;
		}

		void signalFinish() {
			writeFile(session->responseDir + "/finish", "1");
		}

		void signalFinishWithError() {
			writeFile(session->responseDir + "/finish", "0");
		}
	};

	struct FreePortDebugSupport: public HandshakePerform::DebugSupport {
		Core_SpawningKit_HandshakePerformTest *test;
		HandshakeSession *session;
		AtomicInt expectedStartPort;

		virtual void beginWaitUntilSpawningFinished() {
			expectedStartPort = session->expectedStartPort;
			test->counter++;
		}
	};

	struct CrashingDebugSupport: public HandshakePerform::DebugSupport {
		virtual void beginWaitUntilSpawningFinished() {
			throw RuntimeException("oh no!");
		}
	};

	DEFINE_TEST_GROUP_WITH_LIMIT(Core_SpawningKit_HandshakePerformTest, 80);


	/***** General logic *****/

	TEST_METHOD(1) {
		set_test_name("If the app is generic, it finishes when the app is pingable");

		FreePortDebugSupport debugSupport;
		this->debugSupport = &debugSupport;
		config.genericApp = true;
		init(SPAWN_DIRECTLY);
		debugSupport.test = this;
		debugSupport.session = session.get();
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::execute, this));

		EVENTUALLY(1,
			result = counter == 1;
		);

		server.assign(createTcpServer("127.0.0.1", debugSupport.expectedStartPort.get()),
			NULL, 0);

		EVENTUALLY(1,
			result = counter == 2;
		);
	}

	TEST_METHOD(2) {
		set_test_name("If findFreePort is true, it finishes when the app is pingable");

		FreePortDebugSupport debugSupport;
		this->debugSupport = &debugSupport;
		config.findFreePort = true;
		init(SPAWN_DIRECTLY);
		debugSupport.test = this;
		debugSupport.session = session.get();
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::execute, this));

		EVENTUALLY(1,
			result = counter == 1;
		);

		server.assign(createTcpServer("127.0.0.1", debugSupport.expectedStartPort.get()),
			NULL, 0);

		EVENTUALLY(1,
			result = counter == 2;
		);
	}

	TEST_METHOD(3) {
		set_test_name("It finishes when the app has sent the finish signal");

		init(SPAWN_DIRECTLY);
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::execute, this));

		SHOULD_NEVER_HAPPEN(100,
			result = counter > 0;
		);

		createFile(session->responseDir + "/properties.json",
			createGoodPropertiesJson().toStyledString());
		signalFinish();

		EVENTUALLY(1,
			result = counter == 1;
		);
	}

	TEST_METHOD(10) {
		set_test_name("It raises an error if the process exits prematurely");

		init(SPAWN_DIRECTLY);
		pid = fork();
		if (pid == 0) {
			// Exit child
			_exit(1);
		}

		try {
			execute();
			fail("SpawnException expected");
		} catch (const SpawnException &e) {
			ensure_equals(StaticString(e.what()),
				"The application process exited prematurely.");
		}
	}

	TEST_METHOD(11) {
		set_test_name("It raises an error if the procedure took too long");

		config.startTimeoutMsec = 50;
		init(SPAWN_DIRECTLY);
		pid = fork();
		if (pid == 0) {
			// Exit child
			usleep(1000000);
			_exit(1);
		}

		try {
			execute();
			fail("SpawnException expected");
		} catch (const SpawnException &e) {
			ensure_equals(StaticString(e.what()),
				"A timeout error occurred while spawning an application process.");
		}
	}

	TEST_METHOD(15) {
		set_test_name("In the event of an error, it sets the SPAWNING_KIT_HANDSHAKE_PERFORM step to the errored state");

		CrashingDebugSupport debugSupport;
		this->debugSupport = &debugSupport;
		init(SPAWN_DIRECTLY);

		try {
			execute();
			fail("SpawnException expected");
		} catch (const SpawnException &) {
			ensure_equals(session->journey.getFirstFailedStep(), SPAWNING_KIT_HANDSHAKE_PERFORM);
		}
	}

	TEST_METHOD(16) {
		set_test_name("In the event of an error, the exception contains journey state information from the response directory");

		CrashingDebugSupport debugSupport;
		this->debugSupport = &debugSupport;
		init(SPAWN_DIRECTLY);

		createFile(session->responseDir + "/steps/subprocess_listen/state", "STEP_ERRORED");

		try {
			execute();
			fail("SpawnException expected");
		} catch (const SpawnException &) {
			ensure_equals(session->journey.getStepInfo(SUBPROCESS_LISTEN).state,
				STEP_ERRORED);
		}
	}

	TEST_METHOD(17) {
		set_test_name("In the event of an error, the exception contains subprocess stdout and stderr data");

		Pipe p = createPipe(__FILE__, __LINE__);
		CrashingDebugSupport debugSupport;
		init(SPAWN_DIRECTLY);
		HandshakePerform performer(*session, pid, FileDescriptor(), p.first);
		performer.debugSupport = &debugSupport;

		setPrintAppOutputAsDebuggingMessages(true);
		writeExact(p.second, "hi\n");

		try {
			performer.execute();
			fail("SpawnException expected");
		} catch (const SpawnException &e) {
			ensure_equals(e.getStdoutAndErrData(), "hi\n");
		}
	}

	TEST_METHOD(18) {
		set_test_name("In the event of an error caused by the subprocess, the exception contains messages from"
			" the subprocess as dumped in the response directory");

		init(SPAWN_DIRECTLY);
		pid = fork();
		if (pid == 0) {
			// Exit child
			_exit(1);
		}

		createFile(session->responseDir + "/error/summary", "the summary");
		createFile(session->responseDir + "/error/problem_description.txt", "the <problem>");
		createFile(session->responseDir + "/error/advanced_problem_details", "the advanced problem details");
		createFile(session->responseDir + "/error/solution_description.html", "the <b>solution</b>");

		try {
			execute();
			fail("SpawnException expected");
		} catch (const SpawnException &e) {
			ensure_equals(e.getSummary(), "the summary");
			ensure_equals(e.getProblemDescriptionHTML(), "the &lt;problem&gt;");
			ensure_equals(e.getAdvancedProblemDetails(), "the advanced problem details");
			ensure_equals(e.getSolutionDescriptionHTML(), "the <b>solution</b>");
		}
	}

	TEST_METHOD(19) {
		set_test_name("In the event of success, it loads the journey state information from the response directory");

		init(SPAWN_DIRECTLY);
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::execute, this));

		createFile(session->responseDir + "/properties.json",
			createGoodPropertiesJson().toStyledString());
		createFile(session->responseDir + "/steps/subprocess_listen/state",
			"STEP_PERFORMED");
		signalFinish();

		EVENTUALLY(5,
			result = counter == 1;
		);

		ensure_equals(session->journey.getStepInfo(SUBPROCESS_LISTEN).state, STEP_PERFORMED);
	}


	/***** Success response handling *****/

	TEST_METHOD(30) {
		set_test_name("The result object contains basic information such as FDs and time");

		init(SPAWN_DIRECTLY);
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::execute, this));

		createFile(session->responseDir + "/properties.json",
			createGoodPropertiesJson().toStyledString());
		createFile(session->responseDir + "/steps/subprocess_listen/state",
			"STEP_PERFORMED");
		signalFinish();

		EVENTUALLY(5,
			result = counter == 1;
		);

		ensure_equals(session->result.pid, pid);
		ensure(session->result.spawnStartTime != 0);
		ensure(session->result.spawnEndTime >= session->result.spawnStartTime);
		ensure(session->result.spawnStartTimeMonotonic != 0);
		ensure(session->result.spawnEndTimeMonotonic >= session->result.spawnStartTimeMonotonic);
	}

	TEST_METHOD(31) {
		set_test_name("The result object contains sockets specified in properties.json");

		init(SPAWN_DIRECTLY);
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::execute, this));

		createFile(session->responseDir + "/properties.json",
			createGoodPropertiesJson().toStyledString());
		createFile(session->responseDir + "/steps/subprocess_listen/state",
			"STEP_PERFORMED");
		signalFinish();

		EVENTUALLY(5,
			result = counter == 1;
		);

		ensure_equals(session->result.sockets.size(), 1u);
		ensure_equals(session->result.sockets[0].address, "tcp://127.0.0.1:3000");
		ensure_equals(session->result.sockets[0].protocol, "http");
		ensure_equals(session->result.sockets[0].concurrency, 1);
		ensure(session->result.sockets[0].acceptHttpRequests);
	}

	TEST_METHOD(32) {
		set_test_name("If the app is generic, it automatically registers the free port as a request-handling socket");

		FreePortDebugSupport debugSupport;
		this->debugSupport = &debugSupport;
		config.genericApp = true;
		init(SPAWN_DIRECTLY);
		debugSupport.test = this;
		debugSupport.session = session.get();
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::execute, this));

		EVENTUALLY(1,
			result = counter == 1;
		);
		server.assign(createTcpServer("127.0.0.1", debugSupport.expectedStartPort.get()),
			NULL, 0);
		EVENTUALLY(1,
			result = counter == 2;
		);

		ensure_equals(session->result.sockets.size(), 1u);
		ensure_equals(session->result.sockets[0].address, "tcp://127.0.0.1:" + toString(session->expectedStartPort));
		ensure_equals(session->result.sockets[0].protocol, "http");
		ensure_equals(session->result.sockets[0].concurrency, -1);
		ensure(session->result.sockets[0].acceptHttpRequests);
	}

	TEST_METHOD(33) {
		set_test_name("If findFreePort is true, it automatically registers the free port as a request-handling socket");

		FreePortDebugSupport debugSupport;
		this->debugSupport = &debugSupport;
		config.findFreePort = true;
		init(SPAWN_DIRECTLY);
		debugSupport.test = this;
		debugSupport.session = session.get();
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::execute, this));

		EVENTUALLY(1,
			result = counter == 1;
		);
		server.assign(createTcpServer("127.0.0.1", debugSupport.expectedStartPort.get()),
			NULL, 0);
		EVENTUALLY(1,
			result = counter == 2;
		);

		ensure_equals(session->result.sockets.size(), 1u);
		ensure_equals(session->result.sockets[0].address, "tcp://127.0.0.1:" + toString(session->expectedStartPort));
		ensure_equals(session->result.sockets[0].protocol, "http");
		ensure_equals(session->result.sockets[0].concurrency, -1);
		ensure(session->result.sockets[0].acceptHttpRequests);
	}

	TEST_METHOD(34) {
		set_test_name("It raises an error if we expected the subprocess to create a properties.json,"
			" but the file does not conform to the required format");

		init(SPAWN_DIRECTLY);
		createFile(session->responseDir + "/properties.json", "{ \"sockets\": {} }");
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::signalFinish, this));

		try {
			execute();
			fail("SpawnException expected");
		} catch (const SpawnException &e) {
			ensure(containsSubstring(e.getSummary(), "'sockets' must be an array"));
		}
	}

	TEST_METHOD(35) {
		set_test_name("It raises an error if we expected the subprocess to specify at"
			" least one request-handling socket in properties.json, yet the file does"
			" not specify any");

		Json::Value socket, doc;
		socket["address"] = "tcp://127.0.0.1:3000";
		socket["protocol"] = "http";
		socket["concurrency"] = 1;
		doc["sockets"].append(socket);

		init(SPAWN_DIRECTLY);
		createFile(session->responseDir + "/properties.json", doc.toStyledString());
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::signalFinish, this));

		try {
			execute();
			fail("SpawnException expected");
		} catch (const SpawnException &e) {
			ensure(containsSubstring(e.getSummary(),
				"the application did not report any sockets to receive requests on"));
		}
	}

	TEST_METHOD(36) {
		set_test_name("It raises an error if we expected the subprocess to specify at"
			" least one request-handling socket in properties.json, yet properties.json"
			" does not exist");

		init(SPAWN_DIRECTLY);
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::signalFinish, this));

		try {
			execute();
			fail("SpawnException expected");
		} catch (const SpawnException &e) {
			ensure(containsSubstring(e.getSummary(), "sockets are not supplied"));
		}
	}

	TEST_METHOD(37) {
		set_test_name("It raises an error if we expected the subprocess to specify at"
			" least one preloader command socket in properties.json, yet the file does"
			" not specify any");

		Json::Value socket, doc;
		socket["address"] = "tcp://127.0.0.1:3000";
		socket["protocol"] = "http";
		socket["concurrency"] = 1;
		doc["sockets"].append(socket);

		init(START_PRELOADER);
		createFile(session->responseDir + "/properties.json", doc.toStyledString());
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::signalFinish, this));

		try {
			execute();
			fail("SpawnException expected");
		} catch (const SpawnException &e) {
			ensure(containsSubstring(e.getSummary(),
				"the application did not report any sockets to receive preloader commands on"));
		}
	}

	TEST_METHOD(38) {
		set_test_name("It raises an error if we expected the subprocess to specify at"
			" least one preloader command socket in properties.json, yet properties.json"
			" does not exist");

		init(START_PRELOADER);
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::signalFinish, this));

		try {
			execute();
			fail("SpawnException expected");
		} catch (const SpawnException &e) {
			ensure(containsSubstring(e.getSummary(),
				"sockets are not supplied"));
		}
	}


	/***** Error response handling *****/

	TEST_METHOD(50) {
		set_test_name("It raises an error if the application responded with an error");

		init(SPAWN_DIRECTLY);
		TempThread thr(boost::bind(&Core_SpawningKit_HandshakePerformTest::signalFinishWithError, this));

		try {
			execute();
		} catch (const SpawnException &e) {
			ensure_equals(e.getSummary(), "The web application aborted with an error during startup.");
		}
	}
}
