///////////////////////////////////////////////////////////////////////////////
///
///	\file    IndexedDataset.cpp
///	\author  Paul Ullrich
///	\version July 23, 2019

#include "IndexedDataset.h"
#include "STLStringHelper.h"
#include "DataArray1D.h"
#include "DataArray2D.h"
#include "netcdfcpp.h"
#include "NetCDFUtilities.h"
#include "../contrib/tinyxml2.h"
#include "../contrib/json.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fstream>
#include <iomanip>

#if defined(HYPERION_MPIOMP)
#include <mpi.h>
#endif

///////////////////////////////////////////////////////////////////////////////
// DataObjectInfo
///////////////////////////////////////////////////////////////////////////////

std::string DataObjectInfo::FromNcFile(
	NcFile * ncfile
) {
	// Get attributes, if available
	for (int a = 0; a < ncfile->num_atts(); a++) {
		NcAtt * att = ncfile->get_att(a);
		std::string strAttName = att->name();
		if (strAttName == "units") {
			continue;
		}

		// Define new value of this attribute
		std::string strAttNameTemp = strAttName;
		STLStringHelper::ToLower(strAttNameTemp);

		if ((strAttNameTemp == "conventions") ||
		    (strAttNameTemp == "version") ||
		    (strAttNameTemp == "history")
		) {
			m_mapKeyAttributes.insert(
				AttributeMap::value_type(
					strAttName, att->as_string(0)));
		} else {
			m_mapOtherAttributes.insert(
				AttributeMap::value_type(
					strAttName, att->as_string(0)));
		}
/*
		// Check for consistency across files
		} else {
			AttributeMap::const_iterator iterAttKey =
				m_mapKeyAttributes.find(strAttName);
			AttributeMap::const_iterator iterAttOther =
				m_mapOtherAttributes.find(strAttName);

			if (iterAttKey != m_mapKeyAttributes.end()) {
				if (iterAttKey->second != att->as_string(0)) {
					return std::string("ERROR: NetCDF file \"") + strFilename
						+ std::string("\" has inconsistent value of \"")
						+ strAttName + std::string("\" across files");
				}
			}
			if (iterAttOther != m_mapOtherAttributes.end()) {
				if (iterAttOther->second != att->as_string(0)) {
					return std::string("ERROR: NetCDF file \"") + strFilename
						+ std::string("\" has inconsistent value of \"")
						+ strAttName + std::string("\" across files");
				}
			}
			if ((iterAttKey == m_mapKeyAttributes.end()) &&
			    (iterAttOther == m_mapOtherAttributes.end())
			) {
				return std::string("ERROR: NetCDF file \"") + strFilename
					+ std::string("\" has inconsistent appearance of attribute \"")
					+ strAttName + std::string("\" across files");
			}
		}
*/
	}

	return std::string("");
}

///////////////////////////////////////////////////////////////////////////////

std::string DataObjectInfo::FromNcVar(
	NcVar * var,
	bool fCheckConsistency
) {
	// Get name, if available
	std::string strName = var->name();
	if (!fCheckConsistency) {
		m_strName = strName;
	} else if (strName != m_strName) {
		_EXCEPTION2("Calling DataObjectInfo::FromNcVar with mismatched "
			"variable names: \"%s\" \"%s\"",
			strName.c_str(), m_strName.c_str());
	}

	// Get type, if available
	NcType nctype = var->type();
	if (!fCheckConsistency) {
		m_nctype = nctype;
	} else if (nctype != m_nctype) {
		return std::string("ERROR: Variable \"") + strName
			+ std::string("\" has inconsistent type across files");
	}

	// Get units, if available
	std::string strUnits;
	NcAtt * attUnits = var->get_att("units");
	if (attUnits != NULL) {
		strUnits = attUnits->as_string(0);
	}
	if (!fCheckConsistency) {
		m_strUnits = strUnits;
	} else if (strUnits != m_strUnits) {
		return std::string("ERROR: Variable \"") + strName
			+ std::string("\" has inconsistent units across files");
	}

	// Get attributes, if available
	for (int a = 0; a < var->num_atts(); a++) {
		NcAtt * att = var->get_att(a);
		std::string strAttName = att->name();
		if (strAttName == "units") {
			continue;
		}

		// Define new value of this attribute
		if (!fCheckConsistency) {
			if ((strAttName == "missing_value") ||
			    (strAttName == "comments") ||
			    (strAttName == "long_name") ||
			    (strAttName == "grid_name") ||
			    (strAttName == "grid_type")
			) {
				m_mapKeyAttributes.insert(
					AttributeMap::value_type(
						strAttName, att->as_string(0)));
			} else {
				m_mapOtherAttributes.insert(
					AttributeMap::value_type(
						strAttName, att->as_string(0)));
			}

		// Check for consistency across files
		} else {
			AttributeMap::const_iterator iterAttKey =
				m_mapKeyAttributes.find(strAttName);
			AttributeMap::const_iterator iterAttOther =
				m_mapOtherAttributes.find(strAttName);

			if (iterAttKey != m_mapKeyAttributes.end()) {
				if (iterAttKey->second != att->as_string(0)) {
					return std::string("ERROR: Variable \"") + strName
						+ std::string("\" has inconsistent value of \"")
						+ strAttName + std::string("\" across files");
				}
			}
			if (iterAttOther != m_mapOtherAttributes.end()) {
				if (iterAttOther->second != att->as_string(0)) {
					return std::string("ERROR: Variable \"") + strName
						+ std::string("\" has inconsistent value of \"")
						+ strAttName + std::string("\" across files");
				}
			}
			if ((iterAttKey == m_mapKeyAttributes.end()) &&
			    (iterAttOther == m_mapOtherAttributes.end())
			) {
				return std::string("ERROR: Variable \"") + strName
					+ std::string("\" has inconsistent appearance of attribute \"")
					+ strAttName + std::string("\" across files");
			}
		}
	}

	return std::string("");
}

///////////////////////////////////////////////////////////////////////////////

void DataObjectInfo::RemoveRedundantOtherAttributes(
	const DataObjectInfo & doiMaster
) {
	AttributeMap::const_iterator iterattr =
		doiMaster.m_mapOtherAttributes.begin();
	for (; iterattr != doiMaster.m_mapOtherAttributes.end(); iterattr++) {
		m_mapOtherAttributes.erase(iterattr->first);
	}
}

///////////////////////////////////////////////////////////////////////////////
// SubAxis
///////////////////////////////////////////////////////////////////////////////

std::string SubAxis::ValuesToString() const {
	std::ostringstream ssText;

	// No type
	if (m_nctype == ncNoType) {
		return std::string("[ ]");

	// Double type
	} else if (m_nctype == ncDouble) {
		ssText << std::setprecision(17);
		ssText << "[";
		for (int i = 0; i < m_dValuesDouble.size(); i++) {
			ssText << m_dValuesDouble[i];
			if (i != m_dValuesDouble.size()-1) {
				ssText << " ";
			}
		}
		ssText << "]";

	// Float type
	} else if (m_nctype == ncFloat) {
		ssText << std::setprecision(8);
		ssText << "[";
		for (int i = 0; i < m_dValuesFloat.size(); i++) {
			ssText << m_dValuesFloat[i];
			if (i != m_dValuesFloat.size()-1) {
				ssText << " ";
			}
		}
		ssText << "]";

	} else {
		_EXCEPTIONT("Invalid type");
	}

	return ssText.str();
}

///////////////////////////////////////////////////////////////////////////////

void SubAxis::ValuesToJSON(
	nlohmann::json & j
) const {

	// No type
	if (m_nctype == ncNoType) {

	// Double type
	} else if (m_nctype == ncDouble) {
		for (int i = 0; i < m_dValuesDouble.size(); i++) {
			j.push_back(m_dValuesDouble[i]);
		}

	// Float type
	} else if (m_nctype == ncFloat) {
		for (int i = 0; i < m_dValuesFloat.size(); i++) {
			j.push_back(m_dValuesFloat[i]);
		}

	} else {
		_EXCEPTIONT("Invalid type");
	}

}

///////////////////////////////////////////////////////////////////////////////

bool SubAxis::operator==(const SubAxis & dimrange) const {

	// Check for consistent types
	if (dimrange.m_nctype != m_nctype) {
		return false;
	}

	// No type
	if (m_nctype == ncNoType) {
		return true;

	// Dimension values stored as doubles
	} else if (m_nctype == ncDouble) {
		if (dimrange.m_dValuesDouble.size() != m_dValuesDouble.size()) {
			return false;
		}
		for (size_t s = 0; s < m_dValuesDouble.size(); s++) {
			if (!fpa::almost_equal<double>(dimrange.m_dValuesDouble[s], m_dValuesDouble[s])) {
				return false;
			}
		}

	// Dimension values stored as floats
	} else if (m_nctype == ncFloat) {
		if (dimrange.m_dValuesFloat.size() != m_dValuesFloat.size()) {
			return false;
		}
		for (size_t s = 0; s < m_dValuesDouble.size(); s++) {
			if (!fpa::almost_equal<float>(dimrange.m_dValuesFloat[s], m_dValuesFloat[s])) {
				return false;
			}
		}

	// Unhandled type
	} else {
		_EXCEPTIONT("Unhandled type");
	}

	return true;

}

///////////////////////////////////////////////////////////////////////////////
// AxisNameVector
///////////////////////////////////////////////////////////////////////////////

std::string AxisNameVector::ToString() const {
	std::string strAxes("[");
	for (int d = 0; d < size(); d++) {
		strAxes += "\"" + (*this)[d] + "\"";
		if (d != size()-1) {
			strAxes += ", ";
		}
	}
	strAxes += "]";
	return strAxes;
}

///////////////////////////////////////////////////////////////////////////////

void AxisNameVector::ToJSON(nlohmann::json & j) const {
	for (int d = 0; d < size(); d++) {
		j.push_back((*this)[d]);
	}
}

///////////////////////////////////////////////////////////////////////////////
// SubAxisToFileIdMap
///////////////////////////////////////////////////////////////////////////////

std::string SubAxisToFileIdMap::ToString() const {
	std::string strSubAxes("[");
	SubAxisToFileIdMap::const_iterator iterSubAxisToFileId = begin();
	for (; iterSubAxisToFileId != end(); iterSubAxisToFileId++) {
		if (iterSubAxisToFileId != begin()) {
			strSubAxes += ", ";
		}
		strSubAxes += "[";
		for (int d = 0; d < iterSubAxisToFileId->first.size(); d++) {
			strSubAxes += "\"" + iterSubAxisToFileId->first[d] + "\"";
			strSubAxes += ", ";
		}
		strSubAxes += "\"" + iterSubAxisToFileId->second + "\"]";
	}
	strSubAxes += "]";
	return strSubAxes;
}

///////////////////////////////////////////////////////////////////////////////

void SubAxisToFileIdMap::ToJSON(nlohmann::json & j) const {
	SubAxisToFileIdMap::const_iterator iterSubAxisToFileId = begin();
	for (; iterSubAxisToFileId != end(); iterSubAxisToFileId++) {
		nlohmann::json jx;
		for (int d = 0; d < iterSubAxisToFileId->first.size(); d++) {
			jx.push_back(iterSubAxisToFileId->first[d]);
		}
		jx.push_back(iterSubAxisToFileId->second);
		j.push_back(jx);
	}
}


///////////////////////////////////////////////////////////////////////////////
// IndexedDataset
///////////////////////////////////////////////////////////////////////////////

const size_t IndexedDataset::InvalidFileIx = (-1);

///////////////////////////////////////////////////////////////////////////////

const size_t IndexedDataset::InvalidTimeIx = (-1);

///////////////////////////////////////////////////////////////////////////////

const long IndexedDataset::InconsistentDimensionSizes = (-1);

///////////////////////////////////////////////////////////////////////////////

IndexedDataset::~IndexedDataset() {
	for (int v = 0; v < m_vecVariableInfo.size(); v++) {
		delete m_vecVariableInfo[v];
	}
	for (int d = 0; d < m_vecAxisInfo.size(); d++) {
		delete m_vecAxisInfo[d];
	}
}

///////////////////////////////////////////////////////////////////////////////

std::string IndexedDataset::PopulateFromSearchString(
	const std::string & strSearchString
) {
	std::string strBaseDir;

	// File the directory in the search string
	std::string strFileSearchString;
	for (int i = strSearchString.length(); i >= 0; i--) {
		if (strSearchString[i] == '/') {
			strBaseDir = strSearchString.substr(0,i+1);
			strFileSearchString =
				strSearchString.substr(i+1, std::string::npos);
			break;
		}
	}
	if ((strBaseDir == "") && (strFileSearchString == "")) {
		strFileSearchString = strSearchString;
		strBaseDir = "./";
	}

	// Open the directory
	DIR * pDir = opendir(strBaseDir.c_str());
	if (pDir == NULL) {
		return std::string("Unable to open directory \"")
			+ strBaseDir + std::string("\"");
	}

	// Search all files in the directory for match to search string
	std::vector<std::string> vecFilenames;
	struct dirent * pDirent;
	while ((pDirent = readdir(pDir)) != NULL) {
		std::string strFilename = pDirent->d_name;
		if (STLStringHelper::WildcardMatch(
				strFileSearchString.c_str(),
				strFilename.c_str())
		) {
			// File found, insert into list of filenames
			vecFilenames.push_back(strFilename);
		}
	}
	closedir(pDir);

	// Index the variable data
	return IndexVariableData(strBaseDir, vecFilenames);
}

///////////////////////////////////////////////////////////////////////////////

std::string IndexedDataset::LoadData_float(
	const std::string & strVariableName,
	const std::vector<long> & vecAuxIndices,
	DataArray1D<float> & data
) {
/*
	// Find the VariableInfo structure for this Variable
	size_t iVarInfo = 0;
	for (; iVarInfo < m_vecVariableInfo.size(); iVarInfo++) {
		if (strVariableName == m_vecVariableInfo[iVarInfo]->m_strName) {
			break;
		}
	}
	if (iVarInfo == m_vecVariableInfo.size()) {
		_EXCEPTION1("Variable \"%s\" not found in file_list index",
			strVariableName.c_str());
	}

	// Find the local file/time pair associated with this global time index
	VariableInfo & varinfo = *(m_vecVariableInfo[iVarInfo]);

	// Get the time index
	if ((varinfo.m_iTimeDimIx != (-1)) &&
		(varinfo.m_iTimeDimIx >= vecAuxIndices.size())
	) {
		_EXCEPTIONT("time index exceeds auxiliary index size");
	}

	// Extract the global time index
	size_t sTime = (-1);
	if (varinfo.m_iTimeDimIx != (-1)) {
		sTime = vecAuxIndices[varinfo.m_iTimeDimIx];
	}

	// Find local file/time index
	VariableTimeFileMap::const_iterator iter =
		varinfo.m_mapTimeFile.find(sTime);

	if (iter == varinfo.m_mapTimeFile.end()) {
		_EXCEPTION2("sTime (%s) (%lu) not found", strVariableName.c_str(), sTime);
	}

	size_t sFile = iter->second.first;
	int iTime = iter->second.second;

	{
		std::string strLoading =
			std::string("READ [") + strVariableName + std::string("]");

		for (size_t d = 0; d < vecAuxIndices.size(); d++) {
			if (d == varinfo.m_iTimeDimIx) {
				strLoading +=
					std::string(" [")
					+ m_vecTimes[sTime].ToString()
					+ std::string("]");
			} else {
				strLoading +=
					std::string(" [")
					+ varinfo.m_vecAuxDimNames[d]
					+ std::string(": ")
					+ std::to_string(vecAuxIndices[d])
					+ std::string("]");
			}
		}
		Announce(strLoading.c_str());
	}

	// Open the correct NetCDF file
	std::string strFullFilename = m_strBaseDir + m_vecFilenames[sFile];
	NcFile ncfile(strFullFilename.c_str());
	if (!ncfile.is_valid()) {
		_EXCEPTION1("Cannot open file \"%s\"", strFullFilename.c_str());
	}

	// Get the correct variable from the file
	NcVar * var = ncfile.get_var(strVariableName.c_str());
	const long nDims = var->num_dims() - m_vecGridDimNames.size();

	if (var == NULL) {
		_EXCEPTION1("Variable \"%s\" no longer found in file",
			strVariableName.c_str());
	}
	if ((var->type() != ncFloat) && (var->type() != ncDouble)) {
		return std::string("Variable \"")
			+ strVariableName
			+ std::string("\" is not of type float or double");
	}
	if (nDims != vecAuxIndices.size()) {
		_EXCEPTION2("Auxiliary index array size mismatch (%li / %lu)",
			nDims, vecAuxIndices.size());
	}

	// Set the data position and size
	long lTotalSize = 1;
	std::vector<long> vecPos = vecAuxIndices;
	std::vector<long> vecSize = vecAuxIndices;

	for (size_t d = 0; d < vecAuxIndices.size(); d++) {
		if (d == varinfo.m_iTimeDimIx) {
			vecPos[d] = iTime;
		}
		vecSize[d] = 1;
	}
	for (size_t d = 0; d < m_vecGridDimNames.size(); d++) {
		NcDim * dimGrid = var->get_dim(vecPos.size());
		vecPos.push_back(0);
		vecSize.push_back(dimGrid->size());
		lTotalSize *= dimGrid->size();
	}

	if (data.GetRows() != lTotalSize) {
		//for (int d = 0; d < vecAuxIndices.size(); d++) {
		//	printf("%s %i\n", varinfo.m_vecDimNames[d].c_str(), vecAuxIndices[d]);
		//}
		//for (int d = 0; d < nDims; d++) {
		//	printf("%i %i\n", vecPos[d], vecSize[d]);
		//}
		_EXCEPTION2("Data size mismatch (%i/%lu)", data.GetRows(), lTotalSize);
	}

	// Set the position
	var->set_cur(&(vecPos[0]));

	// Load the data
	if (var->type() == ncDouble) {
		DataArray1D<double> data_dbl(data.GetRows());
		var->get(&(data_dbl[0]), &(vecSize[0]));
		for (int i = 0; i < data.GetRows(); i++) {
			data[i] = static_cast<float>(data_dbl[i]);
		}
	} else {
		var->get(&(data[0]), &(vecSize[0]));
	}

	NcError err;
	if (err.get_err() != NC_NOERR) {
		_EXCEPTION1("NetCDF Fatal Error (%i)", err.get_err());
	}

	// Cleanup
	ncfile.close();
*/
	return std::string("");
}

///////////////////////////////////////////////////////////////////////////////

std::string IndexedDataset::WriteData_float(
	const std::string & strVariableName,
	const std::vector<long> & vecAuxIndices,
	const DataArray1D<float> & data
) {
/*
	// Find the VariableInfo structure for this Variable
	size_t iVarInfo = 0;
	for (; iVarInfo < m_vecVariableInfo.size(); iVarInfo++) {
		if (strVariableName == m_vecVariableInfo[iVarInfo]->m_strName) {
			break;
		}
	}

	// File index for write
	size_t sFile = (-1);
	int iLocalTime = (-1);

	// Not found
	if (iVarInfo == m_vecVariableInfo.size()) {
		_EXCEPTION();
	}

	// Get file index from varinfo
	VariableInfo & varinfo = *(m_vecVariableInfo[iVarInfo]);

	// Check auxiliary indices
	if (vecAuxIndices.size() != varinfo.m_vecAuxDimSizes.size()) {
		_EXCEPTIONT("Invalid auxiliary index");
	}

	// Extract the time index
	size_t sTime = (-1);
	if (varinfo.m_iTimeDimIx != (-1)) {
		sTime = vecAuxIndices[varinfo.m_iTimeDimIx];
	}

	// Write message
	{
		std::string strWriting =
			std::string("WRITE [") + strVariableName + std::string("]");

		for (size_t d = 0; d < vecAuxIndices.size(); d++) {
			if (d == varinfo.m_iTimeDimIx) {
				strWriting +=
					std::string(" [")
					+ m_vecTimes[sTime].ToString()
					+ std::string("]");
			} else {
				strWriting +=
					std::string(" [")
					+ varinfo.m_vecAuxDimNames[d]
					+ std::string(": ")
					+ std::to_string(vecAuxIndices[d])
					+ std::string("]");
			}
		}
		Announce(strWriting.c_str());
	}

	// Check consistency with indexing procedure
	if ((sTime == (-1)) && (varinfo.m_iTimeDimIx != (-1))) {
		_EXCEPTIONT("Attempting to write single time index "
			"for multi-time-index variable");
	}
	if ((sTime != (-1)) && (varinfo.m_iTimeDimIx == (-1))) {
		_EXCEPTIONT("Attempting to write single time indexed "
			"variable as multi-indexed variable");
	}

	VariableTimeFileMap::const_iterator iterTimeFile =
		varinfo.m_mapTimeFile.find(sTime);

	if (iterTimeFile == varinfo.m_mapTimeFile.end()) {
		if (sTime == InvalidTimeIx) {
			sFile = m_sReduceTargetIx;
			iLocalTime = (-1);

		} else {
			std::map<size_t, LocalFileTimePair>::const_iterator iterFileIx =
				m_mapOutputTimeFile.find(sTime);

			if (iterFileIx == m_mapOutputTimeFile.end()) {
				_EXCEPTIONT("Unable to determine output file");
			}

			sFile = iterFileIx->second.first;
			iLocalTime = iterFileIx->second.second;

			varinfo.m_mapTimeFile.insert(
				VariableTimeFileMap::value_type(
					sTime,
					LocalFileTimePair(sFile, iLocalTime)));
		}

	} else {
		sFile = iterTimeFile->second.first;
		iLocalTime = iterTimeFile->second.second;
	}

	if (sFile == (-1)) {
		_EXCEPTIONT("Logic error");
	}

	// Set the data position and size
	long lTotalSize = 1;
	std::vector<long> vecPos = vecAuxIndices;
	std::vector<long> vecSize = vecAuxIndices;

	for (size_t d = 0; d < vecAuxIndices.size(); d++) {
		if (d == varinfo.m_iTimeDimIx) {
			vecPos[d] = iLocalTime;
		}
		vecSize[d] = 1;
	}
	for (size_t d = 0; d < m_vecGridDimNames.size(); d++) {
		AxisInfoMap::const_iterator iter =
			m_mapAxisInfo.find(m_vecGridDimNames[d]);

		if (iter == m_mapAxisInfo.end()) {
			_EXCEPTIONT("Dimension not found in map");
		}

		vecPos.push_back(0);
		vecSize.push_back(iter->second.m_lSize);
		lTotalSize *= iter->second.m_lSize;
	}
	//for (size_t d = 0; d < m_vecGridDimNames.size(); d++) {
	//	printf("DIM %s %i/%i\n", varinfo.m_vecDimNames[d].c_str(), vecPos[d], vecSize[d]);
	//}

	if (data.GetRows() != lTotalSize) {
		_EXCEPTION2("Data size mismatch (%i/%lu)", data.GetRows(), lTotalSize);
	}

	// Write data
	std::string strFullFilename = m_strBaseDir + m_vecFilenames[sFile];
	NcFile ncout(strFullFilename.c_str(), NcFile::Write);
	if (!ncout.is_valid()) {
		_EXCEPTION1("Unable to open output file \"%s\"", strFullFilename.c_str());
	}

	// Get dimensions
	std::vector<NcDim *> vecDims;
	vecDims.resize(varinfo.m_vecDimNames.size());
	for (int d = 0; d < vecDims.size(); d++) {
		long lDimSize = GetDimensionSize(varinfo.m_vecDimNames[d]);
		vecDims[d] = ncout.get_dim(varinfo.m_vecDimNames[d].c_str());
		if (vecDims[d] != NULL) {
			if (d != varinfo.m_iTimeDimIx) {
				if (vecDims[d]->size() != lDimSize) {
					_EXCEPTION3("Dimension %s mismatch (%lu / %li)",
						varinfo.m_vecDimNames[d].c_str(),
						vecDims[d]->size(), lDimSize);
				}
			}

		} else {

			// Create a new dimension in output file
			vecDims[d] =
				ncout.add_dim(
					varinfo.m_vecDimNames[d].c_str(),
					lDimSize);

			if (vecDims[d] == NULL) {
				_EXCEPTION1("Cannot add dimension %s",
					varinfo.m_vecDimNames[d].c_str());
			}

			AxisInfoMap::const_iterator iterDimInfo =
				m_mapAxisInfo.find(varinfo.m_vecDimNames[d]);

			// Create a dimension variable in output file
			if (iterDimInfo->second.m_dValuesDouble.size() != 0) {
				NcVar * varDim =
					ncout.add_var(
						varinfo.m_vecDimNames[d].c_str(),
						ncDouble,
						vecDims[d]);
				if (varDim == NULL) {
					_EXCEPTION1("Cannot add variable %s",
						varinfo.m_vecDimNames[d].c_str());
				}
				varDim->set_cur((long)0);
				varDim->put(
					&(iterDimInfo->second.m_dValuesDouble[0]),
					iterDimInfo->second.m_dValuesDouble.size());

				if (iterDimInfo->second.m_strUnits != "") {
					varDim->add_att("units",
						iterDimInfo->second.m_strUnits.c_str());
				}
			}
		}
	}

	// Create variable
	NcVar * var = ncout.get_var(strVariableName.c_str());
	if (var == NULL) {
		var = ncout.add_var(
			strVariableName.c_str(),
			ncFloat,
			vecDims.size(),
			(const NcDim **)(&(vecDims[0])));

		if (var == NULL) {
			_EXCEPTION1("Unable to create variable \"%s\"",
				strVariableName.c_str());
		}

		var->add_att("units", varinfo.m_strUnits.c_str());
	}

	// Set current position
	var->set_cur(&(vecPos[0]));

	// Write data
	var->put(&(data[0]), &(vecSize[0]));
*/
	return std::string("");
}

///////////////////////////////////////////////////////////////////////////////

std::string IndexedDataset::IndexVariableData(
	const std::string & strBaseDir,
	const std::vector<std::string> & vecFilenames
) {
	std::string strError;

	// Check if we're appending to an already populated IndexedDataset
	bool fAppendIndex =
		(m_vecVariableInfo.size() != 0) ||
		(m_vecAxisInfo.size() != 0) ||
		(m_vecFileInfo.size() != 0);

	// Open all files
	for (size_t f = 0; f < vecFilenames.size(); f++) {

		// Open the NetCDF file
		std::string strFullFilename = strBaseDir + vecFilenames[f];
		NcFile ncFile(strFullFilename.c_str(), NcFile::ReadOnly);
		if (!ncFile.is_valid()) {
			return std::string("Unable to open data file \"")
				+ strFullFilename + std::string("\" for reading");
		}

		printf("Indexing %s\n", strFullFilename.c_str());

		// Load in global attributes
		if (m_vecFileInfo.size() == 0) {
			strError = m_datainfo.FromNcFile(&ncFile);
			if (strError != "") return strError;
		}

		// Add a new FileInfo descriptor
		size_t sFileIndex = m_vecFileInfo.size();
		std::string strFileId = std::to_string((long long)sFileIndex);
		m_vecFileInfo.insert(
			strFileId,
			new FileInfo(strFullFilename));
		FileInfo & fileinfo = *(m_vecFileInfo[sFileIndex]);
		fileinfo.FromNcFile(&ncFile);
		fileinfo.RemoveRedundantOtherAttributes(m_datainfo);

		// time indices stored in this file
		std::vector<size_t> vecFileTimeIndices;
/*
		// Find the time variable, if it exists
		NcVar * varTime = ncFile.get_var(m_strRecordDimName.c_str());
		if (varTime != NULL) {
			if (varTime->num_dims() != 1) {
				return std::string("\"")
					+ m_strRecordDimName
					+ std::string("\" variable must contain exactly one dimension in \"")
					+ vecFilenames[f] + std::string("\"");
			}
			if ((varTime->type() != ncInt) && (varTime->type() != ncDouble)) {
				return std::string("\"")
					+ m_strRecordDimName
					+ std::string("\" variable must be ncInt or ncDouble in \"")
					+ vecFilenames[f] + std::string("\"");
			}

			Time::CalendarType timecal;

			NcDim * dimTime = varTime->get_dim(0);
			if (dimTime == NULL) {
				_EXCEPTION1("Malformed NetCDF file \"%s\"",
					vecFilenames[f].c_str());
			}

			// Get calendar
			NcAtt * attTimeCalendar = varTime->get_att("calendar");
			if (attTimeCalendar == NULL) {
				timecal = Time::CalendarStandard;
			} else {
				std::string strTimeCalendar = attTimeCalendar->as_string(0);
				timecal = Time::CalendarTypeFromString(strTimeCalendar);
				if (timecal == Time::CalendarUnknown) {
					return std::string("Unknown calendar \"") + strTimeCalendar
						+ std::string("\" in \"")
						+ vecFilenames[f] + std::string("\"");
				}
			}

			// Get units attribute
			NcAtt * attTimeUnits = varTime->get_att("units");
			if (attTimeUnits != NULL) {
				m_strTimeUnits = attTimeUnits->as_string(0);
			}
			if (m_strTimeUnits == "") {
				return std::string("Unknown units for \"")
					+ m_strRecordDimName
					+ std::string("\" in \"")
					+ vecFilenames[f] + std::string("\"");
			}

			// Add Times to master array and store corresponding indices
			// in vecFileTimeIndices.
			DataArray1D<int> nTimes(dimTime->size());
			if (varTime->type() == ncInt) {
				varTime->set_cur((long)0);
				varTime->get(&(nTimes[0]), dimTime->size());
			}

			DataArray1D<double> dTimes(dimTime->size());
			if (varTime->type() == ncDouble) {
				varTime->set_cur((long)0);
				varTime->get(&(dTimes[0]), dimTime->size());
			}

			for (int t = 0; t < dimTime->size(); t++) {
				Time time(timecal);
				if (m_strTimeUnits != "") {
					if (varTime->type() == ncInt) {
						time.FromCFCompliantUnitsOffsetInt(
							m_strTimeUnits,
							nTimes[t]);

					} else if (varTime->type() == ncDouble) {
						time.FromCFCompliantUnitsOffsetDouble(
							m_strTimeUnits,
							dTimes[t]);
					}
				}

				std::map<Time, size_t>::const_iterator iterTime =
					m_mapTimeToIndex.find(time);

				if (iterTime == m_mapTimeToIndex.end()) {
					size_t sNewIndex = m_vecTimes.size();
					m_vecTimes.push_back(time);
					vecFileTimeIndices.push_back(sNewIndex);
					m_mapTimeToIndex.insert(
						std::pair<Time, size_t>(time, sNewIndex));
				} else {
					vecFileTimeIndices.push_back(iterTime->second);
				}
			}
		}

		printf("..File contains %lu times\n", vecFileTimeIndices.size());
*/
		// Index all Dimensions
		printf("..Loading dimensions\n");
		const int nDims = ncFile.num_dims();
		for (int d = 0; d < nDims; d++) {
			NcDim * dim = ncFile.get_dim(d);
			std::string strAxisName(dim->name());
			AxisInfoMap::iterator iterDim =
				m_mapAxisInfo.find(strAxisName);

			//printf("....Dimension %i (%s)\n", d, strAxisName.c_str());

			// New variable, not yet indexed
			bool fNewAxis = false;

			// Find the corresponding AxisInfo structure
			size_t sDimIndex = 0;
			for (; sDimIndex < m_vecAxisInfo.size(); sDimIndex++) {
				if (strAxisName == m_vecAxisInfo[sDimIndex]->m_strName) {
					break;
				}
			}
			if (sDimIndex == m_vecAxisInfo.size()) {
				m_vecAxisInfo.push_back(
					new AxisInfo(strAxisName));

				fNewAxis = true;
			}

			AxisInfo & axisinfo = *(m_vecAxisInfo[sDimIndex]);

			// Dimension size
			long lSize = dim->size();

			// Create a new SubAxis
			SubAxis * psubaxis = new SubAxis();
			if (psubaxis == NULL) {
				_EXCEPTIONT("Error allocating new SubAxis");
			}
			std::string strSubAxisId =
				std::to_string((long long)axisinfo.m_vecSubAxis.size());

			psubaxis->m_lSize = lSize;

			// Check for variable
			NcVar * varDim = ncFile.get_var(strAxisName.c_str());
			if ((varDim == NULL) && (axisinfo.m_nctype != ncNoType)) {
				return std::string("ERROR: Dimension variable \"")
					+ strAxisName
					+ std::string("\" missing from file, but present in other files.");
			}
			if (varDim != NULL) {
				if (varDim->num_dims() != 1) {
					return std::string("ERROR: Dimension variable \"")
						+ varDim->name()
						+ std::string("\" must have exactly 1 dimension");
				}
				if (std::string(varDim->get_dim(0)->name()) != strAxisName) {
					return std::string("ERROR: Dimension variable \"")
						+ varDim->name()
						+ std::string("\" does not have dimension \"");
						+ varDim->name()
						+ std::string("\"");
				}

				// Set or verify the axis type
				if (fNewAxis) {
					axisinfo.m_nctype = varDim->type();

				} else if (axisinfo.m_nctype != varDim->type()) {
					return std::string("ERROR: Dimension variable \"")
						+ varDim->name()
						+ std::string("\" type mismatch.  Possible"
						" duplicate dimension name in dataset.");
				}
				psubaxis->m_nctype = axisinfo.m_nctype;

				// Check for units attribute
				axisinfo.FromNcVar(varDim, !fNewAxis);

				// Initialize the DataObjectInfo from the NcVar
				strError = psubaxis->FromNcVar(varDim, false);
				if (strError != "") {
					delete psubaxis;
					return strError;
				}

				// Get the values from the dimension
				double dOrder = 0;
				bool fMonotonicityError = false;

				if (axisinfo.m_nctype == ncDouble) {
					psubaxis->m_dValuesDouble.resize(lSize);
					varDim->set_cur((long)0);
					varDim->get(&(psubaxis->m_dValuesDouble[0]), lSize);

				} else if (axisinfo.m_nctype == ncFloat) {
					psubaxis->m_dValuesFloat.resize(lSize);
					varDim->set_cur((long)0);
					varDim->get(&(psubaxis->m_dValuesFloat[0]), lSize);

				} else {
					_EXCEPTIONT("Unsupported dimension nctype");
				}
			}

			// Check if SubAxis already exists
			AxisInfo::SubAxisVector::iterator iterSubAxis =
				axisinfo.m_vecSubAxis.begin();

			for (; iterSubAxis != axisinfo.m_vecSubAxis.end(); iterSubAxis++) {
				if ((**iterSubAxis) == (*psubaxis)) {
					break;
				}
			}
			if (iterSubAxis != axisinfo.m_vecSubAxis.end()) {
				strSubAxisId = iterSubAxis.key();
				delete psubaxis;
			} else {
				axisinfo.m_vecSubAxis.insert(strSubAxisId, psubaxis);
			}

			// Add axis/subaxis pair to FileInfo
			fileinfo.m_mapAxisSubAxis.insert(
				AxisSubAxisPair(strAxisName, strSubAxisId));
		}

		// Loop over all Variables
		printf("..Loading variables\n");
		const int nVariables = ncFile.num_vars();
		for (int v = 0; v < nVariables; v++) {
			NcVar * var = ncFile.get_var(v);
			if (var == NULL) {
				_EXCEPTION1("Malformed NetCDF file \"%s\"",
					vecFilenames[f].c_str());
			}

			std::string strVariableName = var->name();

			// Don't index dimension variables
			bool fDimensionVar = false;
			for (int d = 0; d < m_vecAxisInfo.size(); d++) {
				if (m_vecAxisInfo[d]->m_strName == strVariableName) {
					fDimensionVar = true;
					break;
				}
			}
			if (fDimensionVar) {
				continue;
			}

			//printf("....Variable %i (%s)\n", v, strVariableName.c_str());

			// New variable, not yet indexed
			bool fNewVariable = false;

			// Find the corresponding VariableInfo structure
			size_t sVarIndex = 0;
			for (; sVarIndex < m_vecVariableInfo.size(); sVarIndex++) {
				if (strVariableName == m_vecVariableInfo[sVarIndex]->m_strName) {
					break;
				}
			}
			if (sVarIndex == m_vecVariableInfo.size()) {
				m_vecVariableInfo.push_back(
					new VariableInfo(strVariableName));

				fNewVariable = true;
			}

			VariableInfo & varinfo = *(m_vecVariableInfo[sVarIndex]);

			// Initialize the DataObjectInfo from the NcVar
			strError = varinfo.FromNcVar(var, !fNewVariable);
			if (strError != "") return strError;

			// Build the SubAxisCoordinate
			AxisNameVector vecAxisNames;
			SubAxisIdVector vecSubAxisIds;
			const int nDims = var->num_dims();
			for (int d = 0; d < nDims; d++) {
				std::string strAxisName = var->get_dim(d)->name();
				vecAxisNames.push_back(strAxisName);

				AxisSubAxisMap::const_iterator iterSubAxisId =
					fileinfo.m_mapAxisSubAxis.find(strAxisName);
				if (iterSubAxisId == fileinfo.m_mapAxisSubAxis.end()) {
					_EXCEPTIONT("Logic error");
				}
				vecSubAxisIds.push_back(iterSubAxisId->second);
			}

			// Get subaxis to file id map
			AxisNamesToSubAxisToFileIdMapMap::iterator iterAxisToFileIdMap =
				varinfo.m_mapSubAxisToFileIdMaps.find(vecAxisNames);
			if (iterAxisToFileIdMap == varinfo.m_mapSubAxisToFileIdMaps.end()) {
				std::pair<AxisNamesToSubAxisToFileIdMapMap::iterator, bool> pri =
					varinfo.m_mapSubAxisToFileIdMaps.insert(
						AxisNamesToSubAxisToFileIdMapMap::value_type(
							vecAxisNames,
							SubAxisToFileIdMap()));

				if (!pri.second) {
					_EXCEPTIONT("Logic error");
				}

				iterAxisToFileIdMap = pri.first;
			}
			SubAxisToFileIdMap & mapSubAxisToFileId =
				iterAxisToFileIdMap->second;

			// Insert this subaxis into the map
			std::pair<SubAxisToFileIdMap::iterator, bool> prj =
				mapSubAxisToFileId.insert(
					SubAxisToFileIdMap::value_type(
						vecSubAxisIds,
						strFileId));
/*
			// Load dimension information
			const int nDims = var->num_dims();

			varinfo.m_vecDimNames.resize(nDims);
			varinfo.m_vecDimSizes.resize(nDims);
			for (int d = 0; d < nDims; d++) {
				varinfo.m_vecDimNames[d] = var->get_dim(d)->name();

				if (varinfo.m_vecDimNames[d] == m_strRecordDimName) {
					if (varinfo.m_iTimeDimIx == (-1)) {
						varinfo.m_iTimeDimIx = d;
					} else if (info.m_iTimeDimIx != d) {
						return std::string("ERROR: Variable \"") + strVariableName
							+ std::string("\" has inconsistent \"time\" dimension across files");
					}
					varinfo.m_vecDimSizes[d] = (-1);
				} else {
					varinfo.m_vecDimSizes[d] = var->get_dim(d)->size();
				}

				if ((varinfo.m_vecDimNames[d] == "lev") ||
					(varinfo.m_vecDimNames[d] == "pres") ||
					(varinfo.m_vecDimNames[d] == "z") ||
					(varinfo.m_vecDimNames[d] == "plev")
				) {
					if (varinfo.m_iVerticalDimIx != (-1)) {
						if (varinfo.m_iVerticalDimIx != d) {
							return std::string("ERROR: Possibly multiple vertical dimensions in variable ")
								+ info.m_strName;
						}
					}
					varinfo.m_iVerticalDimIx = d;
				}
			}
*/
/*
			// Determine direction of vertical dimension
			if (info.m_iVerticalDimIx != (-1)) {
				AxisInfoMap::iterator iterDimInfo =
					m_mapAxisInfo.find(info.m_vecDimNames[info.m_iVerticalDimIx]);

				if (iterDimInfo != m_mapAxisInfo.end()) {
					info.m_nVerticalDimOrder = iterDimInfo->second.m_nOrder;

				} else {
					_EXCEPTIONT("Logic error");
				}
			}
*/
/*
			// No time information on this Variable
			if (info.m_iTimeDimIx == (-1)) {
				if (info.m_mapTimeFile.size() == 0) {
					info.m_mapTimeFile.insert(
						std::pair<size_t, LocalFileTimePair>(
							InvalidTimeIx,
							LocalFileTimePair(f, 0)));

				} else if (info.m_mapTimeFile.size() == 1) {
					VariableTimeFileMap::const_iterator iterTimeFile =
						info.m_mapTimeFile.begin();

					if (iterTimeFile->first != InvalidTimeIx) {
						return std::string("Variable \"") + strVariableName
							+ std::string("\" has inconsistent \"time\" dimension across files");
					}

				} else {
					return std::string("Variable \"") + strVariableName
						+ std::string("\" has inconsistent \"time\" dimension across files");
				}

			// Add file and time indices to VariableInfo
			} else {
				for (int t = 0; t < vecFileTimeIndices.size(); t++) {
					VariableTimeFileMap::const_iterator iterTimeFile =
						info.m_mapTimeFile.find(vecFileTimeIndices[t]);

					if (iterTimeFile == info.m_mapTimeFile.end()) {
						info.m_mapTimeFile.insert(
							std::pair<size_t, LocalFileTimePair>(
								vecFileTimeIndices[t],
								LocalFileTimePair(f, t)));

					} else {
						return std::string("Variable \"") + strVariableName
							+ std::string("\" has repeated time across files:\n")
							+ std::string("Time: ") + m_vecTimes[vecFileTimeIndices[t]].ToString() + std::string("\n")
							+ std::string("File1: ") + m_vecFilenames[iterTimeFile->second.first] + std::string("\n")
							+ std::string("File2: ") + m_vecFilenames[f];
					}
				}
			}
*/
		}
	}
/*
	// Sort the Time array
	SortTimeArray();

	for (int v = 0; v < m_vecVariableInfo.size(); v++) {

		VariableInfo & varinfo = *(m_vecVariableInfo[v]);

		// Update the time dimension size for all variables
		int iTimeDimIx = varinfo.m_iTimeDimIx;
		if (iTimeDimIx != (-1)) {
			if (varinfo.m_vecDimSizes.size() < iTimeDimIx) {
				_EXCEPTIONT("Logic error");
			}
			varinfo.m_vecDimSizes[iTimeDimIx] =
				varinfo.m_mapTimeFile.size();
		}

		// Initialize auxiliary dimension information for all variables
		varinfo.m_vecAuxDimNames.clear();
		varinfo.m_vecAuxDimSizes.clear();
		for (size_t d = 0; d < varinfo.m_vecDimSizes.size(); d++) {
			bool fFound = false;
			for (int g = 0; g < m_vecGridDimNames.size(); g++) {
				if (m_vecGridDimNames[g] == varinfo.m_vecDimNames[d]) {
					fFound = true;
					break;
				}
			}
			if (!fFound) {
				varinfo.m_vecAuxDimNames.push_back(
					varinfo.m_vecDimNames[d]);
				varinfo.m_vecAuxDimSizes.push_back(
					varinfo.m_vecDimSizes[d]);
			}
		}
	}
*/
	return std::string("");
}

///////////////////////////////////////////////////////////////////////////////

std::string IndexedDataset::OutputTimeVariableIndexCSV(
	const std::string & strCSVOutputFilename
) {
#if defined(HYPERION_MPIOMP)
	// Only output on root thread
	int nRank;
	MPI_Comm_rank(MPI_COMM_WORLD, &nRank);
	if (nRank != 0) {
		return std::string("");
	}
#endif
/*
	if (m_vecTimes.size() != m_mapTimeToIndex.size()) {
		_EXCEPTIONT("vecTimes / mapTimeToIndex mismatch");
	}

	std::vector< std::pair<size_t,int> > iTimeVariableIndex;
	iTimeVariableIndex.resize(m_vecVariableInfo.size());

	// Open output file
	std::ofstream ofOutput(strCSVOutputFilename.c_str());
	if (!ofOutput.is_open()) {
		return std::string("Unable to open output file \"") + strCSVOutputFilename + "\"";
	}

	// Output variables across header
	ofOutput << "time";
	for (int v = 0; v < m_vecVariableInfo.size(); v++) {
		ofOutput << "," << m_vecVariableInfo[v]->m_strName;
	}
	ofOutput << std::endl;

	// Output variables with no time dimension
	ofOutput << "NONE";
	for (size_t v = 0; v < m_vecVariableInfo.size(); v++) {
		//if (m_vecVariableInfo[v]->m_iTimeDimIx == (-1)) {
		//	ofOutput << ",X";
		//} else {
			ofOutput << ",";
		//}
	}
	ofOutput << std::endl;

	// Output variables with time dimension
	for (size_t t = 0; t < m_vecTimes.size(); t++) {
		ofOutput << m_vecTimes[t].ToString();

		for (size_t v = 0; v < m_vecVariableInfo.size(); v++) {

			VariableTimeFileMap::const_iterator iterTimeFile =
				m_vecVariableInfo[v]->m_mapTimeFile.find(t);

			if (iterTimeFile == m_vecVariableInfo[v]->m_mapTimeFile.end()) {
				ofOutput << ",";
			} else {
				ofOutput << "," << iterTimeFile->second.first
					<< ":" << iterTimeFile->second.second;
			}
		}
		ofOutput << std::endl;
	}

	// Output file names
	ofOutput << std::endl << std::endl;

	ofOutput << "file_ix,filename" << std::endl;
	for (size_t f = 0; f < m_vecFilenames.size(); f++) {
		ofOutput << f << ",\"" << m_strBaseDir << m_vecFilenames[f] << "\"" << std::endl;
	}
*/
	return ("");
}

///////////////////////////////////////////////////////////////////////////////

std::string IndexedDataset::ToXMLFile(
	const std::string & strXMLOutputFilename
) const {
	using namespace tinyxml2;
	tinyxml2::XMLDocument xmlDoc;

	// Declaration
	xmlDoc.InsertEndChild(
		xmlDoc.NewDeclaration(
			"xml version=\"1.0\" encoding=\"\""));

	// DOCTYPE
	tinyxml2::XMLUnknown * pdoctype = xmlDoc.NewUnknown("DOCTYPE dataset SYSTEM \"http://www-pcmdi.llnl.gov/software/cdms/cdml.dtd\"");
	xmlDoc.InsertEndChild(pdoctype);

	// Dataset 
	tinyxml2::XMLElement * pdata = xmlDoc.NewElement("dataset");
	{
		AttributeMap::const_iterator iterAttKey =
			m_datainfo.m_mapKeyAttributes.begin();
		for (; iterAttKey != m_datainfo.m_mapKeyAttributes.end(); iterAttKey++) {
			pdata->SetAttribute(iterAttKey->first.c_str(), iterAttKey->second.c_str());
		}

		AttributeMap::const_iterator iterAttOther =
			m_datainfo.m_mapOtherAttributes.begin();
		for (; iterAttOther != m_datainfo.m_mapOtherAttributes.end(); iterAttOther++) {
			tinyxml2::XMLElement * pattr = xmlDoc.NewElement("attr");
			pattr->SetAttribute("name", iterAttOther->first.c_str());
			pattr->SetAttribute("datatype", "String");
			pattr->SetText(iterAttOther->second.c_str());
			pdata->InsertEndChild(pattr);
		}
	}
	xmlDoc.InsertEndChild(pdata);

	// FileInfo
	LookupVectorHeap<std::string, FileInfo>::const_iterator iterfile = m_vecFileInfo.begin();
	for (; iterfile != m_vecFileInfo.end(); iterfile++) {
		const FileInfo * pfileinfo = *iterfile;

		tinyxml2::XMLElement * pfile = xmlDoc.NewElement("file");
		pfile->SetAttribute("id", iterfile.key().c_str());
		pfile->SetAttribute("name", pfileinfo->m_strFilename.c_str());
		pdata->InsertEndChild(pfile);

		AttributeMap::const_iterator iterAttKey =
			pfileinfo->m_mapKeyAttributes.begin();
		for (; iterAttKey != pfileinfo->m_mapKeyAttributes.end(); iterAttKey++) {
			pfile->SetAttribute(iterAttKey->first.c_str(), iterAttKey->second.c_str());
		}

		AttributeMap::const_iterator iterAttOther =
			pfileinfo->m_mapOtherAttributes.begin();
		for (; iterAttOther != pfileinfo->m_mapOtherAttributes.end(); iterAttOther++) {
			tinyxml2::XMLElement * pattr = xmlDoc.NewElement("attr");
			pattr->SetAttribute("name", iterAttOther->first.c_str());
			pattr->SetAttribute("datatype", "String");
			pattr->SetText(iterAttOther->second.c_str());
			pfile->InsertEndChild(pattr);
		}

		AxisSubAxisMap::const_iterator iterAxes =
			pfileinfo->m_mapAxisSubAxis.begin();
		for (; iterAxes != pfileinfo->m_mapAxisSubAxis.end(); iterAxes++) {
			tinyxml2::XMLElement * paxissubaxis = xmlDoc.NewElement("subaxis");
			paxissubaxis->SetAttribute("axis", iterAxes->first.c_str());
			paxissubaxis->SetAttribute("subaxis", iterAxes->second.c_str());
			pfile->InsertEndChild(paxissubaxis);
		}
	}

	// AxisInfo
	for (int d = 0; d < m_vecAxisInfo.size(); d++) {
		const AxisInfo * paxisinfo = m_vecAxisInfo[d];

		tinyxml2::XMLElement * pdim = xmlDoc.NewElement("axis");
		pdim->SetAttribute("id", paxisinfo->m_strName.c_str());
		pdim->SetAttribute("units", paxisinfo->m_strUnits.c_str());
		pdim->SetAttribute("datatype", NcTypeToString(paxisinfo->m_nctype).c_str());

		AttributeMap::const_iterator iterAttKey =
			paxisinfo->m_mapKeyAttributes.begin();
		for (; iterAttKey != paxisinfo->m_mapKeyAttributes.end(); iterAttKey++) {
			pdim->SetAttribute(iterAttKey->first.c_str(), iterAttKey->second.c_str());
		}

		AttributeMap::const_iterator iterAttOther =
			paxisinfo->m_mapOtherAttributes.begin();
		for (; iterAttOther != paxisinfo->m_mapOtherAttributes.end(); iterAttOther++) {
			tinyxml2::XMLElement * pattr = xmlDoc.NewElement("attr");
			pattr->SetAttribute("name", iterAttOther->first.c_str());
			pattr->SetAttribute("datatype", "String");
			pattr->SetText(iterAttOther->second.c_str());
			pdim->InsertEndChild(pattr);
		}

		// Add all subaxes
		AxisInfo::SubAxisVector::const_iterator itersubaxis = paxisinfo->m_vecSubAxis.begin();
		for (; itersubaxis != paxisinfo->m_vecSubAxis.end(); itersubaxis++) {
			const SubAxis * psubaxisinfo = *itersubaxis;

			tinyxml2::XMLElement * psubaxis;
			if (paxisinfo->m_vecSubAxis.size() == 1) {
				psubaxis = pdim;
			} else {
				psubaxis = xmlDoc.NewElement("subaxis");
				psubaxis->SetAttribute("id", itersubaxis.key().c_str());
				psubaxis->SetAttribute("size", std::to_string((long long)psubaxisinfo->m_lSize).c_str());
			}
			if (psubaxisinfo->m_nctype != ncNoType) {
				psubaxis->SetText(psubaxisinfo->ValuesToString().c_str());
			}
			pdim->InsertEndChild(psubaxis);
		}
	}

	// Variables
	for (int v = 0; v < m_vecVariableInfo.size(); v++) {
		const VariableInfo * pvarinfo = m_vecVariableInfo[v];

		tinyxml2::XMLElement * pvar = xmlDoc.NewElement("variable");
		pvar->SetAttribute("id", pvarinfo->m_strName.c_str());
		pvar->SetAttribute("datatype", NcTypeToString(pvarinfo->m_nctype).c_str());
		pvar->SetAttribute("units", pvarinfo->m_strUnits.c_str());

		AttributeMap::const_iterator iterAttKey =
			pvarinfo->m_mapKeyAttributes.begin();
		for (; iterAttKey != pvarinfo->m_mapKeyAttributes.end(); iterAttKey++) {
			pvar->SetAttribute(iterAttKey->first.c_str(), iterAttKey->second.c_str());
		}

		AttributeMap::const_iterator iterAttOther =
			pvarinfo->m_mapOtherAttributes.begin();
		for (; iterAttOther != pvarinfo->m_mapOtherAttributes.end(); iterAttOther++) {
			tinyxml2::XMLElement * pattr = xmlDoc.NewElement("attr");
			pattr->SetAttribute("name", iterAttOther->first.c_str());
			pattr->SetAttribute("datatype", "String");
			pattr->SetText(iterAttOther->second.c_str());
			pvar->InsertEndChild(pattr);
		}

		// Output subaxis lookup table
		if (pvarinfo->m_mapSubAxisToFileIdMaps.size() != 0) {
			
			AxisNamesToSubAxisToFileIdMapMap::const_iterator iterAxisGroup =
				pvarinfo->m_mapSubAxisToFileIdMaps.begin();
			for (; iterAxisGroup != pvarinfo->m_mapSubAxisToFileIdMaps.end(); iterAxisGroup++) {

				tinyxml2::XMLElement * paxgrp = pvar;
				if (pvarinfo->m_mapSubAxisToFileIdMaps.size() > 1) {
					paxgrp = xmlDoc.NewElement("axisgroup");
				}

				tinyxml2::XMLElement * pnames = xmlDoc.NewElement("axisids");
				pnames->SetText(iterAxisGroup->first.ToString().c_str());
				paxgrp->InsertEndChild(pnames);

				tinyxml2::XMLElement * paxmap = xmlDoc.NewElement("subaxismap");
				paxmap->SetText(iterAxisGroup->second.ToString().c_str());

				paxgrp->InsertEndChild(paxmap);

				if (pvarinfo->m_mapSubAxisToFileIdMaps.size() > 1) {
					pvar->InsertEndChild(paxgrp);
				}
			}
		}

		pdata->InsertEndChild(pvar);
	}

	xmlDoc.SaveFile(strXMLOutputFilename.c_str());

	return std::string("");
}

///////////////////////////////////////////////////////////////////////////////

std::string IndexedDataset::FromJSONFile(
	const std::string & strJSONInput
) {
	return std::string("");
}

///////////////////////////////////////////////////////////////////////////////

std::string IndexedDataset::ToJSONFile(
	const std::string & strJSONOutputFilename,
	bool fPrettyPrint
) const {
	std::ofstream ofJSON(strJSONOutputFilename.c_str());
	if (!ofJSON.is_open()) {
		_EXCEPTION1("Error opening file \"%s\" for writing",
			strJSONOutputFilename.c_str());
	}

	nlohmann::json j;

	// Dataset 
	nlohmann::json & jd = j["dataset"];
	{
		AttributeMap::const_iterator iterAttKey =
			m_datainfo.m_mapKeyAttributes.begin();
		for (; iterAttKey != m_datainfo.m_mapKeyAttributes.end(); iterAttKey++) {
			jd[iterAttKey->first.c_str()] = iterAttKey->second.c_str();
		}

		AttributeMap::const_iterator iterAttOther =
			m_datainfo.m_mapOtherAttributes.begin();
		for (; iterAttOther != m_datainfo.m_mapOtherAttributes.end(); iterAttOther++) {
			jd[iterAttOther->first.c_str()] = iterAttOther->second.c_str();
		}
	}

	// FileInfo
	nlohmann::json & jf = j["file"];
	LookupVectorHeap<std::string, FileInfo>::const_iterator iterfile = m_vecFileInfo.begin();
	for (; iterfile != m_vecFileInfo.end(); iterfile++) {
		const FileInfo * pfileinfo = *iterfile;

		nlohmann::json & jfi = jf[iterfile.key().c_str()];

		jfi["name"] = pfileinfo->m_strFilename.c_str();

		AttributeMap::const_iterator iterAttKey =
			pfileinfo->m_mapKeyAttributes.begin();
		for (; iterAttKey != pfileinfo->m_mapKeyAttributes.end(); iterAttKey++) {
			jfi[iterAttKey->first.c_str()] = iterAttKey->second.c_str();
		}

		AttributeMap::const_iterator iterAttOther =
			pfileinfo->m_mapOtherAttributes.begin();
		for (; iterAttOther != pfileinfo->m_mapOtherAttributes.end(); iterAttOther++) {
			jfi[iterAttOther->first.c_str()] = iterAttOther->second.c_str();
		}

		nlohmann::json & jfia = jfi["axes"];
		AxisSubAxisMap::const_iterator iterAxes =
			pfileinfo->m_mapAxisSubAxis.begin();
		for (; iterAxes != pfileinfo->m_mapAxisSubAxis.end(); iterAxes++) {
			nlohmann::json jaxis;
			jaxis.push_back(iterAxes->first.c_str());
			jaxis.push_back(iterAxes->second.c_str());
			jfia.push_back(jaxis);
		}

	}

	// AxisInfo
	nlohmann::json & ja = j["axes"];
	for (int d = 0; d < m_vecAxisInfo.size(); d++) {
		const AxisInfo * paxisinfo = m_vecAxisInfo[d];

		nlohmann::json & jaa = ja[paxisinfo->m_strName.c_str()];
		jaa["units"] = paxisinfo->m_strUnits.c_str();
		jaa["datatype"] =  NcTypeToString(paxisinfo->m_nctype).c_str();

		AttributeMap::const_iterator iterAttKey =
			paxisinfo->m_mapKeyAttributes.begin();
		for (; iterAttKey != paxisinfo->m_mapKeyAttributes.end(); iterAttKey++) {
			jaa[iterAttKey->first.c_str()] = iterAttKey->second.c_str();
		}

		AttributeMap::const_iterator iterAttOther =
			paxisinfo->m_mapOtherAttributes.begin();
		for (; iterAttOther != paxisinfo->m_mapOtherAttributes.end(); iterAttOther++) {
			jaa[iterAttOther->first.c_str()] = iterAttOther->second.c_str();
		}

		// Add all subaxes
		AxisInfo::SubAxisVector::const_iterator itersubaxis = paxisinfo->m_vecSubAxis.begin();
		for (; itersubaxis != paxisinfo->m_vecSubAxis.end(); itersubaxis++) {
			const SubAxis * psubaxisinfo = *itersubaxis;

			nlohmann::json * jaas = NULL;
			if (paxisinfo->m_vecSubAxis.size() == 1) {
				jaas = &jaa;
			} else {
				jaas = &(jaa[itersubaxis.key().c_str()]);
				(*jaas)["size"] = psubaxisinfo->m_lSize;
			}
			if (psubaxisinfo->m_nctype != ncNoType) {
				psubaxisinfo->ValuesToJSON((*jaas)["values"]);
			}
		}
	}

	// Variables
	nlohmann::json & jv = j["variables"];
	for (int v = 0; v < m_vecVariableInfo.size(); v++) {
		const VariableInfo * pvarinfo = m_vecVariableInfo[v];

		nlohmann::json & jvv = jv[pvarinfo->m_strName.c_str()];
		jvv["units"] = pvarinfo->m_strUnits.c_str();
		jvv["datatype"] = NcTypeToString(pvarinfo->m_nctype).c_str();

		AttributeMap::const_iterator iterAttKey =
			pvarinfo->m_mapKeyAttributes.begin();
		for (; iterAttKey != pvarinfo->m_mapKeyAttributes.end(); iterAttKey++) {
			jvv[iterAttKey->first.c_str()] = iterAttKey->second.c_str();
		}

		AttributeMap::const_iterator iterAttOther =
			pvarinfo->m_mapOtherAttributes.begin();
		for (; iterAttOther != pvarinfo->m_mapOtherAttributes.end(); iterAttOther++) {
			jvv[iterAttOther->first.c_str()] = iterAttOther->second.c_str();
		}

		// Output subaxis lookup table
		if (pvarinfo->m_mapSubAxisToFileIdMaps.size() != 0) {

			AxisNamesToSubAxisToFileIdMapMap::const_iterator iterAxisGroup =
				pvarinfo->m_mapSubAxisToFileIdMaps.begin();
			for (; iterAxisGroup != pvarinfo->m_mapSubAxisToFileIdMaps.end(); iterAxisGroup++) {

				nlohmann::json * jvvg = NULL;
				if (pvarinfo->m_mapSubAxisToFileIdMaps.size() > 1) {
					jvvg = &(jvv["axisgroup"]);
				} else {
					jvvg = &jvv;
				}

				iterAxisGroup->first.ToJSON((*jvvg)["axisids"]);

				iterAxisGroup->second.ToJSON((*jvvg)["subaxismap"]);
			}
		}
	}

	// Output to the file stream
	if (fPrettyPrint) {
		ofJSON << std::setw(4) << j;
	} else {
		ofJSON << j;
	}

	return std::string("");
}

///////////////////////////////////////////////////////////////////////////////

