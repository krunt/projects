package org;

import java.io.ByteArrayOutputStream;

public class TSKVUtils {
	public static byte[] quote(byte [] b, int length) {
		ByteArrayOutputStream rowBuilder = new ByteArrayOutputStream();
		for (int i = 0; i < length; ++i) {
			switch (b[i]) {
			case '\t':
			rowBuilder.write('\\');
			rowBuilder.write('t');
			break;
			
			case '\n':
			rowBuilder.write('\\');
			rowBuilder.write('n');
			break;
			
			case '\\':
			rowBuilder.write('\\');
			rowBuilder.write('\\');
			break;
			
			case '\r':
			rowBuilder.write('\\');
			rowBuilder.write('r');
			break;
			
			case '\0':
			rowBuilder.write('\\');
			rowBuilder.write('0');
			break;
			
			default:
			rowBuilder.write(b[i]);
			};
		}
		return rowBuilder.toByteArray();	
	}
	
	public static byte [] dequote(byte [] ent, int offset, int length) {
		ByteArrayOutputStream rowBuilder = new ByteArrayOutputStream();
		for (int i = offset; i < offset + length; ++i) {
			
			if (i + 1 < offset + length && ent[i] == '\\') {
			
				switch (ent[i+1]) {
				case '\\':
				rowBuilder.write('\\');
				++i;
				break;
				
				case 'n':
				rowBuilder.write('\n');
				++i;
				break;
				
				case 't':
				rowBuilder.write('\t');
				++i;
				break;
				
				case 'r':
				rowBuilder.write('\r');
				++i;
				break;
				
				case '0':
				rowBuilder.write('\0');
				++i;
				break;
				
				default:
				rowBuilder.write(ent[i]);
				};
				
			} else {
				rowBuilder.write(ent[i]);
			}
		}
		return rowBuilder.toByteArray();		
	}
}
