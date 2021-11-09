#include "test.pb.h"
#include "test.cpnp.h"
#include <capnp/message.h>
#include <capnp/serialize-packed.h>

#include <iostream>

std::size_t protoBufSize() {
	ProtobufTest::TestMessage protobufMessage;
//	protobufMessage.set_integer(42);
	protobufMessage.set_text("I am a test");

	std::size_t len = protobufMessage.ByteSizeLong();
	unsigned char *buffer = new unsigned char[len];

	protobufMessage.SerializeToArray(buffer, len);

	delete[] buffer;

	return len;
}

std::size_t cpnprotoSize() {
	::capnp::MallocMessageBuilder builder;

	CapnProtoTest::TestMessage::Builder messageBuilder = builder.initRoot<CapnProtoTest::TestMessage>();
	messageBuilder.setInteger(42);
	messageBuilder.setText("I am a test");

	::kj::VectorOutputStream vectorStream;
	::capnp::writePackedMessage(vectorStream, builder);
	::capnp::writeMessage(vectorStream, builder);

	return vectorStream.getArray().size();
}

int main() {
	std::cout << "Protobuf size:  " << protoBufSize() << std::endl;
	std::cout << "CapnProto size: " << cpnprotoSize() << std::endl;
}
