/*
 * KeyListOps.cpp
 *
 *  Created on: Feb 24, 2014
 *      Author: nek3d
 */
#include "KeyListOps.h"
#include "FileRecordMgr.h"
#include <cmath> //for isnan

KeyListOps::KeyListOps() {
	_opCodes["sum"] = SUM;
	_opCodes["mean"] = MEAN;
	_opCodes["stddev"] = STDDEV;
	_opCodes["sample_stddev"] = SAMPLE_STDDEV;
	_opCodes["median"] = MEDIAN;
	_opCodes["mode"] = MODE;
	_opCodes["antimode"] = ANTIMODE;
	_opCodes["min"] = MIN;
	_opCodes["max"] = MAX;
	_opCodes["absmin"] = ABSMIN;
	_opCodes["absmax"] = ABSMAX;
	_opCodes["count"] = COUNT;
	_opCodes["distinct"] = DISTINCT;
	_opCodes["count_distinct"] = COUNT_DISTINCT;
	_opCodes["distinct_only"] = DISTINCT_ONLY;
	_opCodes["collapse"] = COLLAPSE;
	_opCodes["concat"] = CONCAT;
	_opCodes["freq_asc"] = FREQ_ASC;
	_opCodes["freq_desc"] = FREQ_DESC;
	_opCodes["first"] = FIRST;
	_opCodes["last"] = LAST;

	_isNumericOp[SUM] = true;
	_isNumericOp[MEAN] = true;
	_isNumericOp[STDDEV] = true;
	_isNumericOp[MEDIAN] = true;
	_isNumericOp[MODE] = false;
	_isNumericOp[ANTIMODE] = false;
	_isNumericOp[MIN] = true;
	_isNumericOp[MAX] = true;
	_isNumericOp[ABSMIN] = true;
	_isNumericOp[COUNT] = false;
	_isNumericOp[DISTINCT] = false;
	_isNumericOp[COUNT_DISTINCT] = false;
	_isNumericOp[DISTINCT_ONLY] = false;
	_isNumericOp[COLLAPSE] = false;
	_isNumericOp[CONCAT] = false;
	_isNumericOp[FREQ_ASC] = false;
	_isNumericOp[FREQ_DESC] = false;
	_isNumericOp[FIRST] = false;
	_isNumericOp[LAST] = false;

	_methods.setDelimStr(",");
	_methods.setNullValue(".");

	// default to BED score column
	_columns = "5";
	// default to "sum"
	_operations = "sum";

}

bool KeyListOps::isNumericOp(OP_TYPES op) const {
	map<OP_TYPES, bool>::const_iterator iter = _isNumericOp.find(op);
	return (iter == _isNumericOp.end() ? false : iter->second);
}

bool KeyListOps::isNumericOp(const QuickString &op) const {
	return isNumericOp(getOpCode(op));
}

KeyListOps::OP_TYPES KeyListOps::getOpCode(const QuickString &operation) const {
	//If the operation does not exist, return INVALID.
	//otherwise, return code for given operation.
	map<QuickString, OP_TYPES>::const_iterator iter = _opCodes.find(operation);
	if (iter == _opCodes.end()) {
		return INVALID;
	}
	return iter->second;
}


bool KeyListOps::isValidColumnOps(FileRecordMgr *dbFile) {

    if (dbFile->getFileType() == FileRecordTypeChecker::BAM_FILE_TYPE) {
         //throw Error
        cerr << endl << "*****" << endl
             << "***** ERROR: BAM database file not currently supported for column operations."
             << endl;
        exit(1);
    }


	//get the strings from context containing the comma-delimited lists of columns
	//and operations. Split both of these into vectors. Get the operation code
	//for each operation string. Finally, make a vector of pairs, where the first
	//member of each pair is a column number, and the second member is the code for the
	//operation to perform on that column.

    Tokenizer colTokens;
    Tokenizer opsTokens;

    int numCols = colTokens.tokenize(_columns, ',');
	int numOps = opsTokens.tokenize(_operations, ',');

	if (numOps < 1 || numCols < 1) {
		 cerr << endl << "*****" << endl
		             << "***** ERROR: There must be at least one column and at least one operation named." << endl;
		 return false;
	}
	if (numOps > 1 && numCols > 1 && numCols != numOps) {
		 cerr << endl << "*****" << endl
		             << "***** ERROR: There are " << numCols <<" columns given, but there are " << numOps << " operations." << endl;
		cerr << "\tPlease provide either a single operation that will be applied to all listed columns, " << endl;
		cerr << "\ta single column to which all operations will be applied," << endl;
		cerr << "\tor an operation for each column." << endl;
		return false;
	}
	int loop = max(numCols, numOps);
	for (int i=0; i < loop; i++) {
		int col = str2chrPos(colTokens.getElem(numCols > 1 ? i : 0));

		//check that the column number is valid
		if (col < 1 || col > dbFile->getNumFields()) {
			 cerr << endl << "*****" << endl  << "***** ERROR: Requested column " << col << ", but database file "
					 << dbFile->getFileName() << " only has fields 1 - " << dbFile->getNumFields() << "." << endl;
			 return false;
		}
		const QuickString &operation = opsTokens.getElem(numOps > 1 ? i : 0);
		OP_TYPES opCode = getOpCode(operation);
		if (opCode == INVALID) {
			cerr << endl << "*****" << endl
								 << "***** ERROR: " << operation << " is not a valid operation. " << endl;
			return false;
		}
		_colOps.push_back(pair<int, OP_TYPES>(col, opCode));
	}


	//The final step we need to do is check that for each column/operation pair,
	//if the operation is numeric, see if the database's record type supports
	//numeric operations for that column. For instance, we can allow the mean
	//of column 4 for a BedGraph file, because that's numeric, but not for Bed4,
	//because that isn't.

	for (int i = 0; i < (int)_colOps.size(); i++) {
		int col = _colOps[i].first;
		OP_TYPES opCode = _colOps[i].second;
		FileRecordTypeChecker::RECORD_TYPE recordType = dbFile->getRecordType();

		if (isNumericOp(opCode)) {
			bool isValidNumOp = false;
			switch(recordType) {
				case FileRecordTypeChecker::BED3_RECORD_TYPE:
					isValidNumOp = Bed3Interval::isNumericField(col);
					break;

				case FileRecordTypeChecker::BED4_RECORD_TYPE:
					isValidNumOp = Bed4Interval::isNumericField(col);
					break;

				case FileRecordTypeChecker::BED5_RECORD_TYPE:
					isValidNumOp = Bed5Interval::isNumericField(col);
					break;

				case FileRecordTypeChecker::BEDGRAPH_RECORD_TYPE:
					isValidNumOp = BedGraphInterval::isNumericField(col);
					break;

				case FileRecordTypeChecker::BED6_RECORD_TYPE:
					isValidNumOp = Bed6Interval::isNumericField(col);
					break;

				case FileRecordTypeChecker::BED_PLUS_RECORD_TYPE:
					isValidNumOp = BedPlusInterval::isNumericField(col);
					break;

				case FileRecordTypeChecker::BED12_RECORD_TYPE:
					isValidNumOp = Bed12Interval::isNumericField(col);
					break;

				case FileRecordTypeChecker::BAM_RECORD_TYPE:
					isValidNumOp = BamRecord::isNumericField(col);
					break;

				case FileRecordTypeChecker::VCF_RECORD_TYPE:
					isValidNumOp = VcfRecord::isNumericField(col);
					break;

				case FileRecordTypeChecker::GFF_RECORD_TYPE:
					isValidNumOp = GffRecord::isNumericField(col);
					break;

				default:
					break;
			}
			if (!isValidNumOp) {
				 cerr << endl << "*****" << endl  << "***** ERROR: Column " << col << " is not a numeric field for database file "
						 << dbFile->getFileName() << "." << endl;
				 return false;
			}
		}
	}

    return true;
}

const QuickString & KeyListOps::getOpVals(RecordKeyList &hits)
{
	//loop through all requested columns, and for each one, call the method needed
	//for the operation specified.
	_methods.setKeyList(&hits);
	_outVals.clear();
	double val = 0.0;
	for (int i=0; i < (int)_colOps.size(); i++) {
		int col = _colOps[i].first;
		OP_TYPES opCode = _colOps[i].second;

		_methods.setColumn(col);
		switch (opCode) {
		case SUM:
			val = _methods.getSum();
			if (isnan(val)) {
				_outVals.append(_methods.getNullValue());
			} else {
				_outVals.append(val);
			}
			break;

		case MEAN:
			val = _methods.getMean();
			if (isnan(val)) {
				_outVals.append(_methods.getNullValue());
			} else {
				_outVals.append(val);
			}
			break;

		case STDDEV:
			val = _methods.getStddev();
			if (isnan(val)) {
				_outVals.append(_methods.getNullValue());
			} else {
				_outVals.append(val);
			}
			break;

		case SAMPLE_STDDEV:
			val = _methods.getSampleStddev();
			if (isnan(val)) {
				_outVals.append(_methods.getNullValue());
			} else {
				_outVals.append(val);
			}
			break;

		case MEDIAN:
			val = _methods.getMedian();
			if (isnan(val)) {
				_outVals.append(_methods.getNullValue());
			} else {
				_outVals.append(val);
			}
			break;

		case MODE:
			_outVals.append(_methods.getMode());
			break;

		case ANTIMODE:
			_outVals.append(_methods.getAntiMode());
			break;

		case MIN:
			val = _methods.getMin();
			if (isnan(val)) {
				_outVals.append(_methods.getNullValue());
			} else {
				_outVals.append(val);
			}
			break;

		case MAX:
			val = _methods.getMax();
			if (isnan(val)) {
				_outVals.append(_methods.getNullValue());
			} else {
				_outVals.append(val);
			}
			break;

		case ABSMIN:
			val = _methods.getAbsMin();
			if (isnan(val)) {
				_outVals.append(_methods.getNullValue());
			} else {
				_outVals.append(val);
			}
			break;

		case ABSMAX:
			val = _methods.getAbsMax();
			if (isnan(val)) {
				_outVals.append(_methods.getNullValue());
			} else {
				_outVals.append(val);
			}
			break;

		case COUNT:
			_outVals.append(_methods.getCount());
			break;

		case DISTINCT:
			_outVals.append(_methods.getDistinct());
			break;

		case COUNT_DISTINCT:
			_outVals.append(_methods.getCountDistinct());
			break;

		case DISTINCT_ONLY:
			_outVals.append(_methods.getDistinctOnly());
			break;

		case COLLAPSE:
			_outVals.append(_methods.getCollapse());
			break;

		case CONCAT:
			_outVals.append(_methods.getConcat());
			break;

		case FREQ_ASC:
			_outVals.append(_methods.getFreqAsc());
			break;

		case FREQ_DESC:
			_outVals.append(_methods.getFreqDesc());
			break;

		case FIRST:
			_outVals.append(_methods.getFirst());
			break;

		case LAST:
			_outVals.append(_methods.getLast());
			break;

		case INVALID:
		default:
			// Any unrecognized operation should have been handled already in the context validation.
			// It's thus unnecessary to handle it here, but throw an error to help us know if future
			// refactoring or code changes accidentally bypass the validation phase.
			cerr << "ERROR: Invalid operation given for column " << col << ". Exiting..." << endl;
			break;
		}
		//if this isn't the last column, add a tab.
		if (i < (int)_colOps.size() -1) {
			_outVals.append('\t');
		}
	}
	return _outVals;
}

void KeyListOpsHelp() {
    cerr << "\t-c\t"             << "Specify columns from the B file to map onto intervals in A." << endl;
    cerr                         << "\t\tDefault: 5." << endl;
    cerr						<<  "\t\tMultiple columns can be specified in a comma-delimited list." << endl << endl;

    cerr << "\t-o\t"             << "Specify the operation that should be applied to -c." << endl;
    cerr                         << "\t\tValid operations:" << endl;
    cerr                         << "\t\t    sum, min, max, absmin, absmax," << endl;
    cerr                         << "\t\t    mean, median," << endl;
    cerr                         << "\t\t    collapse (i.e., print a delimited list (duplicates allowed)), " << endl;
    cerr                         << "\t\t    distinct (i.e., print a delimited list (NO duplicates allowed)), " << endl;
    cerr                         << "\t\t    count" << endl;
    cerr                         << "\t\t    count_distinct (i.e., a count of the unique values in the column), " << endl;
    cerr                         << "\t\tDefault: sum" << endl;
    cerr						 << "\t\tMultiple operations can be specified in a comma-delimited list." << endl << endl;

    cerr						<< "\t\tIf there is only column, but multiple operations, all operations will be" << endl;
    cerr						<< "\t\tapplied on that column. Likewise, if there is only one operation, but" << endl;
    cerr						<< "\t\tmultiple columns, that operation will be applied to all columns." << endl;
    cerr						<< "\t\tOtherwise, the number of columns must match the the number of operations," << endl;
    cerr						<< "\t\tand will be applied in respective order." << endl;
    cerr						<< "\t\tE.g., \"-c 5,4,6 -o sum,mean,count\" will give the sum of column 5," << endl;
    cerr						<< "\t\tthe mean of column 4, and the count of column 6." << endl;
    cerr						<< "\t\tThe order of output columns will match the ordering given in the command." << endl << endl<<endl;

    cerr << "\t-delim\t"                 << "Specify a custom delimiter for the collapse operations." << endl;
    cerr                                 << "\t\t- Example: -delim \"|\"" << endl;
    cerr                                 << "\t\t- Default: \",\"." << endl << endl;

}
