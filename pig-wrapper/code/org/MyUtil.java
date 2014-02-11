package org;

import org.apache.commons.codec.binary.Base64;
import org.apache.hadoop.mapred.JobConf;
import org.apache.pig.backend.executionengine.ExecException;
import org.apache.pig.backend.hadoop.HDataType;
import org.apache.pig.data.Tuple;
import org.apache.pig.impl.io.NullableTuple;
import org.apache.pig.impl.io.PigNullableWritable;

public class MyUtil {
	
	public static byte[] computeBase64(byte[] b, int length) {
		byte[] bcopy = new byte[length];
		System.arraycopy(b, 0, bcopy, 0, length);
		return Base64.encodeBase64(bcopy);
	}
	
	public static String dumpBytes(byte [] b, int offset, int length) {
		StringBuilder builder = new StringBuilder();
		for (int i = offset; i < offset + length; ++i) {
			builder.append((byte)b[i] & 0x0f);
			builder.append(((byte)b[i] & 0xf0) >> 4);
			builder.append(" ");
		}
		return builder.toString();
	}
	
	public static boolean isSecondarySort(JobConf conf) {
		return conf.get("pig.secondarySortOrder") != null; 
	}
	
	public static boolean isComplexWrite(JobConf conf) {
		return conf.get("pig.reducePlan") != null; 
	}	

	public static String getStackTrace() {
		StackTraceElement[] traces = Thread.currentThread().getStackTrace();
		
		StringBuilder builder = new StringBuilder();
		for (int i = 0; i < traces.length; ++i) {
			builder.append(traces[i].toString());
			builder.append("\n");
		}
		return builder.toString();
	}
	
	public static PigNullableWritable cloneWritable(PigNullableWritable obj, byte type) throws ExecException {
		boolean isNull = obj.isNull();
		byte mIndex = obj.getIndex();
		
		PigNullableWritable objCopy = HDataType.getWritableComparableTypes(
				obj.getValueAsPigType(), type);
		
		objCopy.setNull(isNull);
		objCopy.setIndex(mIndex);
		
		return objCopy;
	}
	
	public static NullableTuple cloneTuple(NullableTuple obj) {
		boolean isNull = obj.isNull();
		byte mIndex = obj.getIndex();
		
		NullableTuple result = new NullableTuple((Tuple)obj.getValueAsPigType());
		
		result.setNull(isNull);
		result.setIndex(mIndex);
		
		return result;
	}
	
	public static NullableTuple cloneTupleOnlyAttributes(NullableTuple obj, Tuple arg) {
		boolean isNull = obj.isNull();
		byte mIndex = obj.getIndex();
		
		NullableTuple result = new NullableTuple(arg);
		
		result.setNull(isNull);
		result.setIndex(mIndex);
		
		return result;
	}
	
	public static boolean joinKeysAreEqual(PigNullableWritable k1, PigNullableWritable k2) {
		if (k1.getIndex() != k2.getIndex()) {
			return false;
		}
		return k1.equals(k2);
	}
	
	
	public static byte[] stringizeByte(byte arg) {
		byte [] x = new byte[2];
		x[0] = (byte) ((byte)arg & 0x0f);
		x[1] =  (byte) (((byte)arg & 0xf0) >> 4);
		x[0] += 'a'; x[1] += 'a';
		return x;
	}
	
	public static byte destringizeByte(byte [] arg) {
		byte a0 = arg[0];
		byte a1 = arg[1];
		a0 -= 'a'; a1 -= 'a';
		return (byte) (((a1 & 0x0f) << 4) + a0);
	}	
	
}

