///////////////////////////////////////////////////////////////////////////////
///
///	\file    autocurator.cpp
///	\author  Paul Ullrich
///	\version January 31, 2019
///
///	<remarks>
///		Copyright 2016- Paul Ullrich
///
///		This file is distributed as part of the Tempest source code package.
///		Permission is granted to use, copy, modify and distribute this
///		source code and its documentation under the terms of the GNU General
///		Public License.  This software is provided "as is" without express
///		or implied warranty.
///	</remarks>

#include "CommandLine.h"
#include "Announce.h"
#include "IndexedDataset.h"

#include <string>

#include "netcdfcpp.h"

#if defined(HYPERION_MPIOMP)
#include <mpi.h>
#endif

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {

#if defined(HYPERION_MPIOMP)
	// Initialize MPI
	MPI_Init(&argc, &argv);
#endif

	// Turn off fatal errors in NetCDF
	NcError error(NcError::silent_nonfatal);

try {

	// Set Announce to only output on head node
	AnnounceOnlyOutputOnRankZero();

	// Path for files
	std::string strFilePath;

	// File extension
	std::string strFileName;

	// Recurse into subfolders
	bool fRecurse;

	// Input JSON file
	std::string strInputFileJSON;

	// Output XML file
	std::string strOutputFileXML;

	// Output JSON file
	std::string strOutputFileJSON;

	// Pretty print
	bool fPrettyPrint;

	// Parse the command line
	BeginCommandLine()
   	CommandLineString(strFilePath, "path", "");
	CommandLineString(strFileName, "ext", "*.nc");
	CommandLineBool(fRecurse, "recurse");
	CommandLineString(strInputFileJSON, "in_json", "");
	CommandLineString(strOutputFileXML, "out_xml", "");
	CommandLineString(strOutputFileJSON, "out_json", "");
	CommandLineBool(fPrettyPrint, "out_pretty");

	ParseCommandLine(argc, argv);
	EndCommandLine(argv)

	// Check arguments
	if ((strFilePath == "") && (strInputFileJSON == "")) {
		_EXCEPTIONT("No --path or --in_json specified");
	}

	// Banner
	AnnounceBanner();

	// Create a new IndexedDataset
	AnnounceStartBlock("Creating IndexedDataset");
	IndexedDataset objFileList("file_list");
	AnnounceEndBlock("Done");

	// Load from JSON file
	if (strInputFileJSON != "") {
		AnnounceStartBlock("Populating IndexedDataset\n");
		objFileList.FromJSONFile(strInputFileJSON);
		AnnounceEndBlock("Done");
	}

	// Populate from search string
	AnnounceStartBlock("Populating IndexedDataset\n");
	//std::string strError = objFileList.PopulateFromSearchString(strFilePath);
	std::string strError =
		objFileList.PopulateFromFilePath(
			strFilePath,
			strFileName,
			fRecurse);

	if (strError != "") {
		std::cout << strError << std::endl;
		return (-1);
	}
	AnnounceEndBlock("Done");
/*
	// Output to CSV file
	AnnounceStartBlock("Output to CSV file\n");
	objFileList.OutputTimeVariableIndexCSV(strOutputFile);
	AnnounceEndBlock("Done");
*/
	// Output to XML file
	if (strOutputFileXML != "") {
		AnnounceStartBlock("Output to XML file\n");
		objFileList.ToXMLFile(strOutputFileXML);
		AnnounceEndBlock("Done");
	}

	// Output to JSON file
	if (strOutputFileJSON != "") {
		AnnounceStartBlock("Output to JSON file\n");
		objFileList.ToJSONFile(strOutputFileJSON, fPrettyPrint);
		AnnounceEndBlock("Done");
	}

	// Banner
	AnnounceBanner();

} catch(Exception & e) {
	Announce(e.ToString().c_str());
} catch(...) {
}

#if defined(HYPERION_MPIOMP)
	// Deinitialize MPI
	MPI_Finalize();
#endif
}

///////////////////////////////////////////////////////////////////////////////

