set(ADM_script_headers
	AudioEncoder.h  AudioOutput.h  AudioOutputCollectionPrototype.h  AudioProperties.h  CheckBoxControl.h
	ComboBoxControl.h  ComboBoxItem.h  ComboBoxItemCollectionPrototype.h  Control.h  ControlCollectionPrototype.h
	Dialog.h  Directory.h  DoubleSpinBoxControl.h  Editor.h  File.h  FileInformation.h  FrameProperties.h  LineEditControl.h
	Muxer.h  	Segment.h  SegmentCollectionPrototype.h  SliderControl.h  SpinBoxControl.h  VideoDecoder.h  VideoEncoder.h
	VideoFileProperties.h  VideoFilter.h  VideoFilterCollectionPrototype.h  VideoOutput.h)

ADM_QT_WRAP_CPP(ADM_qtScript_SRCS ${ADM_script_headers}  QtScriptObject.h  QtScriptConfigObject.h)

set(ADM_script_headers
	${ADM_script_headers}  Doxygen_MainPage.h  Doxygen_Functions.h  Doxygen_Examples.h)

set(ADM_qtScript_SRCS
	${ADM_qtScript_SRCS}  QtScriptEngine.cpp  QtScriptObject.cpp  QtScriptConfigObject.cpp
	QtScriptWriter.cpp  AdmScriptMapper.cpp  AudioEncoder.cpp  AudioOutput.cpp
	AudioOutputCollection.cpp  AudioOutputCollectionPrototype.cpp  AudioProperties.cpp  CheckBoxControl.cpp
	ComboBoxControl.cpp  ComboBoxItem.cpp  Control.cpp  Dialog.cpp  Directory.cpp  DoubleSpinBoxControl.cpp
	Editor.cpp  File.cpp  FileInformation.cpp  FrameProperties.cpp  LineEditControl.cpp  Muxer.cpp  Segment.cpp
	SegmentCollection.cpp  SegmentCollectionPrototype.cpp  SliderControl.cpp  SpinBoxControl.cpp  VideoDecoder.cpp
	VideoEncoder.cpp  VideoFileProperties.cpp  VideoFilter.cpp  VideoFilterCollection.cpp  VideoFilterCollectionPrototype.cpp
	VideoOutput.cpp)

include_directories(
	"${CMAKE_CURRENT_BINARY_DIR}"
	"${QT_INCLUDES}"
	../include
	"${AVIDEMUX_TOP_SOURCE_DIR}/avidemux/common")

add_definitions(-Werror -Wall)
ADD_SCRIPT_ENGINE(adm_script_${QT_LIBRARY_EXTENSION} SHARED ${ADM_qtScript_SRCS})
target_link_libraries(
	adm_script_${QT_LIBRARY_EXTENSION}  ADM_coreAudioEncoder6  ADM_core6  ADM_coreDemuxer6  ADM_coreMuxer6
	ADM_coreUI6  ADM_coreVideoEncoder6  ADM_coreVideoFilter6  ADM_coreUtils6  ADM_coreAudio6
	${QT_QTSCRIPT_LIBRARY}  ${QT_QTCORE_LIBRARY})
INSTALL_SCRIPT_ENGINE_QT4(adm_script_${QT_LIBRARY_EXTENSION})

if (NOT APPLE)
	target_link_libraries(adm_script_${QT_LIBRARY_EXTENSION} -Wl,--version-script,"${CMAKE_CURRENT_SOURCE_DIR}/ADM_script_qt.v")
endif (NOT APPLE)

if (QT_QTSCRIPTTOOLS_FOUND)
    add_definitions(-DQT_SCRIPTTOOLS)
    target_link_libraries(adm_script_${QT_LIBRARY_EXTENSION} ${QT_QTSCRIPTTOOLS_LIBRARY} ${QT_QTGUI_LIBRARY})
endif (QT_QTSCRIPTTOOLS_FOUND)

APPEND_SUMMARY_LIST("Scripting" "QtScript Debugger" "${QT_QTSCRIPTTOOLS_FOUND}")

## == doxygen == ##

# Dirty hacks to try to clean C++ classes so they look more like JS for Doxygen.
# Using CMake's string functions would be easier but unfortunately it has a bug
# when reading files that contain semicolons.

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
configure_file(qtScriptDocs.doxygen.cmake "${CMAKE_CURRENT_BINARY_DIR}/doxygen/qtScriptDocs.doxygen")
find_program(SED_PATH sed)

foreach(sourceFile ${ADM_script_headers})
	get_filename_component(className "${sourceFile}" NAME_WE)

	# remove namespace, constructor and Q_INVOKABLE macro from files
	# C++ types are substituted with Javascript friendly ones (/*% type %*/)

	add_custom_command(
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/doxygen/${sourceFile}"
		COMMAND "${SED_PATH}"
				"-e '/^namespace/,/{/d' \\
				-e 's/Q_INVOKABLE//' \\
				-e 's/ %%\\*\\///g' \\
				-e 's/(void)/()/g' \\
				-e 's/const //g' \\
				-r -e 's/[a-zA-Z0-9_:;\\x26]+ \\/\\*%%//g' \\
				\"${sourceFile}\"" > "${CMAKE_CURRENT_BINARY_DIR}/doxygen/${sourceFile}"
		WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
		DEPENDS "${sourceFile}")

	set(parsedFiles ${parsedFiles} "${CMAKE_CURRENT_BINARY_DIR}/doxygen/${sourceFile}")
endforeach(sourceFile)

add_custom_command(
	OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/html/index.html"
	COMMAND doxygen qtScriptDocs.doxygen
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doxygen"
	DEPENDS ${parsedFiles} qtScriptDocs.doxygen.cmake  header.html  footer.html)

add_custom_target(
    qtScriptDocs
    COMMAND "${CMAKE_COMMAND}" -E remove doxygen.png
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/html/index.html"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/html")

INSTALL_SCRIPT_ENGINE_HELP(QtScript${QT_LIBRARY_EXTENSION} "${CMAKE_CURRENT_SOURCE_DIR}/html/")
