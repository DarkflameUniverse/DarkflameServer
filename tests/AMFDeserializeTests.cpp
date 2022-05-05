#include <chrono>
#include <fstream>
#include <iostream>

#include "AMFDeserialize.h"
#include "AMFFormat.h"
#include "CommonCxxTests.h"

AMFArrayValue* ReadFromBitStream(RakNet::BitStream* bitStream) {
	AMFDeserialize reader;

	return reader.Read(bitStream, true);
}

/**
 * This test checks that if we recieve an unimplemented AMFValueType 
 * we correctly throw an error and can actch it.
 */
void TestUnimplementedAMFValues() {
	std::vector<AMFValueType> unimplementedValues = {
		AMFValueType::AMFInteger,
		AMFValueType::AMFXMLDoc,
		AMFValueType::AMFDate,
		AMFValueType::AMFObject,
		AMFValueType::AMFXML,
		AMFValueType::AMFByteArray,
		AMFValueType::AMFVectorInt,
		AMFValueType::AMFVectorUInt,
		AMFValueType::AMFVectorDouble,
		AMFValueType::AMFVectorObject,
		AMFValueType::AMFDictionary
		};
	// Run unimplemented tests to check that errors are thrown if
	// unimplemented AMF values are attempted to be parsed.
	std::ifstream fileStream;
	fileStream.open("AMFBitStreamUnimplementedTest.bin", std::ios::binary);

	// Read a test BitStream from a file
	std::vector<char> baseBitStream;
	char byte = 0;
	while (fileStream.get(byte)) {
		baseBitStream.push_back(byte);
	}

	fileStream.close();

	for (auto amfValueType : unimplementedValues) {
		RakNet::BitStream testBitStream;
		for (auto element : baseBitStream) {
			testBitStream.Write(element);
		}
		testBitStream.Write(amfValueType);
		bool caughtException = false;
		try {
			ReadFromBitStream(&testBitStream);
		} catch (AMFValueType unimplementedValueType) {
			caughtException = true;
		}
		std::cout << "Testing unimplemented value " << amfValueType << " Did we catch an exception: " << (caughtException ? "YES" : "NO") << std::endl;
		assert(caughtException);
	}
}

void TestLoad() {
	// Now run a load test.
	auto startTime = std::chrono::high_resolution_clock::now();
	std::ifstream fileStream;
	fileStream.open("AMFBitStreamLoadTest.bin", std::ios::binary);

	// Read a test BitStream from a file
	RakNet::BitStream bitStream;
	char byte = 0;
	while (fileStream.get(byte)) {
		bitStream.Write<char>(byte);
	}

	fileStream.close();

	ReadFromBitStream(&bitStream);
	auto endTime = std::chrono::high_resolution_clock::now();

	float timeSpent = std::chrono::duration_cast<std::chrono::duration<float>>(endTime - startTime).count();

	// 1.5f is the max packet processing time we allow.  Since we may have other packets,
	// dont allow this packet to use more than the max time.
	assert(timeSpent < 1.5f);
}

int TestLiveCapture() {
	std::ifstream testFileStream;
	testFileStream.open("AMFBitStreamTest.bin", std::ios::binary);

	// Read a test BitStream from a file
	RakNet::BitStream testBitStream;
	char byte = 0;
	while (testFileStream.get(byte)) {
		testBitStream.Write<char>(byte);
	}

	testFileStream.close();

	auto result = ReadFromBitStream(&testBitStream);

	// Test the outermost array
	ASSERT_EQ(dynamic_cast<AMFStringValue*>(result->FindValue("BehaviorID"))->GetStringValue(), "10447");
	ASSERT_EQ(dynamic_cast<AMFStringValue*>(result->FindValue("objectID"))->GetStringValue(), "288300744895913279")

	// Test the execution state array
	auto executionState = dynamic_cast<AMFArrayValue*>(result->FindValue("executionState"));
	ASSERT_NE(executionState, nullptr);

	auto strips = dynamic_cast<AMFArrayValue*>(executionState->FindValue("strips"))->GetDenseArray();

	ASSERT_EQ(strips.size(), 1);

	auto stripsPosition0 = dynamic_cast<AMFArrayValue*>(strips[0]);

	auto actionIndex = dynamic_cast<AMFDoubleValue*>(stripsPosition0->FindValue("actionIndex"));

	ASSERT_EQ(actionIndex->GetDoubleValue(), 0.0f);

	auto stripIDExecution = dynamic_cast<AMFDoubleValue*>(stripsPosition0->FindValue("id"));

	ASSERT_EQ(stripIDExecution->GetDoubleValue(), 0.0f);

	auto stateIDExecution = dynamic_cast<AMFDoubleValue*>(executionState->FindValue("stateID"));

	ASSERT_EQ(stateIDExecution->GetDoubleValue(), 0.0f);

	auto states = dynamic_cast<AMFArrayValue*>(result->FindValue("states"))->GetDenseArray();

	ASSERT_EQ(states.size(), 1);

	auto firstState = dynamic_cast<AMFArrayValue*>(states[0]);

	auto stateID = dynamic_cast<AMFDoubleValue*>(firstState->FindValue("id"));

	ASSERT_EQ(stateID->GetDoubleValue(), 0.0f);

	auto stripsInState = dynamic_cast<AMFArrayValue*>(firstState->FindValue("strips"))->GetDenseArray();

	ASSERT_EQ(stripsInState.size(), 1);

	auto firstStrip = dynamic_cast<AMFArrayValue*>(stripsInState[0]);

	auto actionsInFirstStrip = dynamic_cast<AMFArrayValue*>(firstStrip->FindValue("actions"))->GetDenseArray();

	ASSERT_EQ(actionsInFirstStrip.size(), 3);

	auto actionID = dynamic_cast<AMFDoubleValue*>(firstStrip->FindValue("id"));

	ASSERT_EQ(actionID->GetDoubleValue(), 0.0f)

	auto uiArray = dynamic_cast<AMFArrayValue*>(firstStrip->FindValue("ui"));

	auto xPos = dynamic_cast<AMFDoubleValue*>(uiArray->FindValue("x"));
	auto yPos = dynamic_cast<AMFDoubleValue*>(uiArray->FindValue("y"));

	ASSERT_EQ(xPos->GetDoubleValue(), 103.0f);
	ASSERT_EQ(yPos->GetDoubleValue(), 82.0f);

	auto stripID = dynamic_cast<AMFDoubleValue*>(firstStrip->FindValue("id"));

	ASSERT_EQ(stripID->GetDoubleValue(), 0.0f)

	auto firstAction = dynamic_cast<AMFArrayValue*>(actionsInFirstStrip[0]);

	auto firstType = dynamic_cast<AMFStringValue*>(firstAction->FindValue("Type"));

	ASSERT_EQ(firstType->GetStringValue(), "OnInteract");

	auto firstCallback = dynamic_cast<AMFStringValue*>(firstAction->FindValue("__callbackID__"));

	ASSERT_EQ(firstCallback->GetStringValue(), "");

	auto secondAction = dynamic_cast<AMFArrayValue*>(actionsInFirstStrip[1]);

	auto secondType = dynamic_cast<AMFStringValue*>(secondAction->FindValue("Type"));

	ASSERT_EQ(secondType->GetStringValue(), "FlyUp");

	auto secondCallback = dynamic_cast<AMFStringValue*>(secondAction->FindValue("__callbackID__"));

	ASSERT_EQ(secondCallback->GetStringValue(), "");

	auto secondDistance = dynamic_cast<AMFDoubleValue*>(secondAction->FindValue("Distance"));

	ASSERT_EQ(secondDistance->GetDoubleValue(), 25.0f);

	auto thirdAction = dynamic_cast<AMFArrayValue*>(actionsInFirstStrip[2]);

	auto thirdType = dynamic_cast<AMFStringValue*>(thirdAction->FindValue("Type"));

	ASSERT_EQ(thirdType->GetStringValue(), "FlyDown");

	auto thirdCallback = dynamic_cast<AMFStringValue*>(thirdAction->FindValue("__callbackID__"));

	ASSERT_EQ(thirdCallback->GetStringValue(), "");

	auto thirdDistance = dynamic_cast<AMFDoubleValue*>(thirdAction->FindValue("Distance"));

	ASSERT_EQ(thirdDistance->GetDoubleValue(), 25.0f);

	return 0;
}

void TestNullStream() {
	auto result = ReadFromBitStream(nullptr);
	assert(result == nullptr);
}

// Ideally we have a positive test case here and a load test case?

int AMFDeserializeTests(int argc, char** const argv) {
	std::cout << "Checking that using a null bitstream doesnt cause exception" << std::endl;
	TestNullStream();
	std::cout << "Passed nullptr test, checking live capture" << std::endl;
	TestLiveCapture();
	std::cout << "Passed live capture, checking load" << std::endl;
	TestLoad();
	std::cout << "Passed load, checking unimplemented amf values" << std::endl;
	TestUnimplementedAMFValues();
	std::cout << "Passed all tests." << std::endl;
	return 0;
}

/**
 * Below is the AMF that is in the AMFBitStreamTest.bin file that we are reading in 
 * from a bitstream to test.
 * args: amf3!
{
	"objectID": "288300744895913279",
	"BehaviorID": "10447",
	"executionState": amf3! 
	{
		"strips": amf3! 
		[
			amf3! 
			{
				"actionIndex": 0.0,
				"id": 0.0,
			},
		],
	"stateID": 0.0,
	},
	"states": amf3!
	[
		amf3!
		{
			"id": 0.0,
			"strips": amf3!
			[
				amf3!
				{
					"actions": amf3!
					[
						amf3!
						{
							"Type": "OnInteract",
							"__callbackID__": "",
						},
						amf3!
						{
							"Distance": 25.0,
							"Type": "FlyUp",
							"__callbackID__": "",
						},
						amf3!
						{
							"Distance": 25.0,
							"Type": "FlyDown",
							"__callbackID__": "",
						},
					],
					"id": 0.0,
					"ui": amf3!
					{
						"x": 103.0,
						"y": 82.0,
					},
				},
			],
		},
	],
}
 */
