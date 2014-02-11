package org;

import java.io.IOException;

import org.apache.hadoop.io.DataInputBuffer;
import org.apache.hadoop.mapred.RawKeyValueIterator;
import org.apache.hadoop.util.Progress;

public class StubRawKeyValueIterator implements RawKeyValueIterator {

	public void close() throws IOException {}
	public DataInputBuffer getKey() throws IOException { return null; }
	public Progress getProgress() { return null; }
	public DataInputBuffer getValue() throws IOException { return null; }
	public boolean next() throws IOException { return false; }
	
}