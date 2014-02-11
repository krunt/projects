package org;

import java.io.IOException;

import org.apache.commons.codec.binary.Base64;
import org.apache.hadoop.io.DataInputBuffer;
import org.apache.hadoop.io.serializer.Deserializer;
import org.apache.hadoop.io.serializer.SerializationFactory;
import org.apache.hadoop.mapred.JobConf;


public class MyBase64Deserializer<T> {
	DataInputBuffer inputBuffer;
	Deserializer<T> deserializer;
	
	MyBase64Deserializer(JobConf conf, Class<T> cls) throws IOException {
		inputBuffer = new DataInputBuffer(); 
		deserializer = new SerializationFactory(conf).getDeserializer(cls);
		deserializer.open(inputBuffer);
	}
	
	public T deserialize(byte [] b, T e) throws IOException {
		byte [] dequoted = TSKVUtils.dequote(b, 0, b.length);
		inputBuffer.reset(dequoted, dequoted.length);
		e = deserializer.deserialize(e);
		return e;
	}
	
	public T deserialize(byte [] b, int length, T e) throws IOException {
		return this.deserialize(b,  0, length, e);
	}
	
	public T deserialize(byte [] b, int start, int length, T e) throws IOException {
		byte [] dequoted = TSKVUtils.dequote(b, start, length);
		inputBuffer.reset(dequoted, dequoted.length);
		e = deserializer.deserialize(e);
		return e;
	}
}


