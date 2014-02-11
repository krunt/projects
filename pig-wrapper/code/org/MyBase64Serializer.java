package org;

import java.io.IOException;

import org.apache.commons.codec.binary.Base64;
import org.apache.hadoop.io.DataOutputBuffer;
import org.apache.hadoop.io.Writable;
import org.apache.hadoop.io.serializer.SerializationFactory;
import org.apache.hadoop.io.serializer.Serializer;
import org.apache.hadoop.mapred.JobConf;

class MyBase64Serializer<T extends Writable> {
	DataOutputBuffer outputBuffer;
	Serializer<T> serializer;
	
	MyBase64Serializer(JobConf conf, Class<T> cls) throws IOException {
		outputBuffer = new DataOutputBuffer();
		
		SerializationFactory factory = new SerializationFactory(conf);
		serializer = factory.getSerializer(cls);
		
		serializer.open(outputBuffer);
	}
	
	public byte[] serialize(T e) throws IOException {
		outputBuffer.reset();
		serializer.serialize(e);
		
		byte [] r = TSKVUtils.quote(outputBuffer.getData(), 
				outputBuffer.getLength());
		
		return r;
	}
}