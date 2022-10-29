#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>

#include "AMFDeserialize.h"
#include "AMFFormat.h"
#include "CommonCxxTests.h"

std::unique_ptr<AMFValue> ReadFromBitStream(RakNet::BitStream* bitStream) {
	AMFDeserialize deserializer;
	std::unique_ptr<AMFValue> returnValue(deserializer.Read(bitStream));
	return returnValue;
}

int ReadAMFUndefinedFromBitStream() {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x00);
	std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), AMFValueType::AMFUndefined);
	return 0;
}

int ReadAMFNullFromBitStream() {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x01);
	std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), AMFValueType::AMFNull);
	return 0;
}

int ReadAMFFalseFromBitStream() {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x02);
	std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), AMFValueType::AMFFalse);
	return 0;
}

int ReadAMFTrueFromBitStream() {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x03);
	std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), AMFValueType::AMFTrue);
	return 0;
}

int ReadAMFIntegerFromBitStream() {
	CBITSTREAM;
	{
		bitStream.Write<uint8_t>(0x04);
		// 127 == 01111111
		bitStream.Write<uint8_t>(127);
		std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), AMFValueType::AMFInteger);
		// Check that the max value of a byte can be read correctly
		ASSERT_EQ(static_cast<AMFIntegerValue*>(res.get())->GetIntegerValue(), 127);
	}
	bitStream.Reset();
	{
		bitStream.Write<uint8_t>(0x04);
		bitStream.Write<uint32_t>(UINT32_MAX);
		std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), AMFValueType::AMFInteger);
		// Check that we can read the maximum value correctly
		ASSERT_EQ(static_cast<AMFIntegerValue*>(res.get())->GetIntegerValue(), 536870911);
	}
	bitStream.Reset();
	{
		bitStream.Write<uint8_t>(0x04);
		// 131 == 10000011
		bitStream.Write<uint8_t>(131);
		// 255 == 11111111
		bitStream.Write<uint8_t>(255);
		// 127 == 01111111
		bitStream.Write<uint8_t>(127);
		std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), AMFValueType::AMFInteger);
		// Check that short max can be read correctly
		ASSERT_EQ(static_cast<AMFIntegerValue*>(res.get())->GetIntegerValue(), UINT16_MAX);
	}
	bitStream.Reset();
	{
		bitStream.Write<uint8_t>(0x04);
		// 255 == 11111111
		bitStream.Write<uint8_t>(255);
		// 127 == 01111111
		bitStream.Write<uint8_t>(127);
		std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), AMFValueType::AMFInteger);
		// Check that 2 byte max can be read correctly
		ASSERT_EQ(static_cast<AMFIntegerValue*>(res.get())->GetIntegerValue(), 16383);
	}
	return 0;
}

int ReadAMFDoubleFromBitStream() {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x05);
	bitStream.Write<double>(25346.4f);
	std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), AMFValueType::AMFDouble);
	ASSERT_EQ(static_cast<AMFDoubleValue*>(res.get())->GetDoubleValue(), 25346.4f);
	return 0;
}

int ReadAMFStringFromBitStream() {
	CBITSTREAM;
	bitStream.Write<uint8_t>(0x06);
	bitStream.Write<uint8_t>(0x0F);
	std::string toWrite = "stateID";
	for (auto e : toWrite) bitStream.Write<char>(e);
	std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
	ASSERT_EQ(res->GetValueType(), AMFValueType::AMFString);
	ASSERT_EQ(static_cast<AMFStringValue*>(res.get())->GetStringValue(), "stateID");
	return 0;
}

int ReadAMFArrayFromBitStream() {
	CBITSTREAM;
	// Test empty AMFArray
	bitStream.Write<uint8_t>(0x09);
	bitStream.Write<uint8_t>(0x01);
	bitStream.Write<uint8_t>(0x01);
	{
		std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), AMFValueType::AMFArray);
		ASSERT_EQ(static_cast<AMFArrayValue*>(res.get())->GetAssociativeMap().size(), 0);
		ASSERT_EQ(static_cast<AMFArrayValue*>(res.get())->GetDenseArray().size(), 0);
	}
	bitStream.Reset();
	// Test a key'd value and dense value
	bitStream.Write<uint8_t>(0x09);
	bitStream.Write<uint8_t>(0x03);
	bitStream.Write<uint8_t>(0x15);
	for (auto e : "BehaviorID") if (e != '\0') bitStream.Write<char>(e);
	bitStream.Write<uint8_t>(0x06);
	bitStream.Write<uint8_t>(0x0B);
	for (auto e : "10447") if (e != '\0') bitStream.Write<char>(e);
	bitStream.Write<uint8_t>(0x01);
	bitStream.Write<uint8_t>(0x06);
	bitStream.Write<uint8_t>(0x0B);
	for (auto e : "10447") if (e != '\0') bitStream.Write<char>(e);
	{
		std::unique_ptr<AMFValue> res(ReadFromBitStream(&bitStream));
		ASSERT_EQ(res->GetValueType(), AMFValueType::AMFArray);
		ASSERT_EQ(static_cast<AMFArrayValue*>(res.get())->GetAssociativeMap().size(), 1);
		ASSERT_EQ(static_cast<AMFArrayValue*>(res.get())->GetDenseArray().size(), 1);
		ASSERT_EQ(static_cast<AMFStringValue*>(static_cast<AMFArrayValue*>(res.get())->FindValue("BehaviorID"))->GetStringValue(), "10447");
		ASSERT_EQ(static_cast<AMFStringValue*>(static_cast<AMFArrayValue*>(res.get())->GetDenseArray()[0])->GetStringValue(), "10447");
	}
	// Test a dense array
	return 0;
}

/**
 * This test checks that if we recieve an unimplemented AMFValueType
 * we correctly throw an error and can actch it.
 */
int TestUnimplementedAMFValues() {
	std::vector<AMFValueType> unimplementedValues = {
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
		ASSERT_EQ(caughtException, true);
	}
	return 0;
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

	auto resultFromFn = ReadFromBitStream(&testBitStream);
	auto result = static_cast<AMFArrayValue*>(resultFromFn.get());
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

int TestNullStream() {
	auto result = ReadFromBitStream(nullptr);
	ASSERT_EQ(result.get(), nullptr);
	return 0;
}

int AMFDeserializeTests(int argc, char** const argv) {
	std::cout << "Checking that using a null bitstream doesnt cause exception" << std::endl;
	if (TestNullStream()) return 1;
	std::cout << "passed nullptr test, checking basic tests" << std::endl;
	if (ReadAMFUndefinedFromBitStream() != 0) return 1;
	if (ReadAMFNullFromBitStream() != 0) return 1;
	if (ReadAMFFalseFromBitStream() != 0) return 1;
	if (ReadAMFTrueFromBitStream() != 0) return 1;
	if (ReadAMFIntegerFromBitStream() != 0) return 1;
	if (ReadAMFDoubleFromBitStream() != 0) return 1;
	if (ReadAMFStringFromBitStream() != 0) return 1;
	if (ReadAMFArrayFromBitStream() != 0) return 1;
	std::cout << "Passed basic test, checking live capture" << std::endl;
	if (TestLiveCapture() != 0) return 1;
	std::cout << "Passed live capture, checking unimplemented amf values" << std::endl;
	if (TestUnimplementedAMFValues() != 0) return 1;
	std::cout << "Passed all tests." << std::endl;
	return 0;
}

/**
 * Below is the AMF that is in the AMFBitStreamTest.bin file that we are reading in
 * from a bitstream to test.
args: amf3!
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
