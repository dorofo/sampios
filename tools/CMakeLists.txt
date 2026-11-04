cmake_minimum_required(VERSION 3.15)
project(sampcli CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../SampIOS/Core)
set(RAKNET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../SampIOS/vendor/raknet)

set(SAMP_SOURCES
        ${CORE_DIR}/SampProtocol.cpp
        ${CORE_DIR}/RakTransport.cpp
        ${CORE_DIR}/PlayerSync.cpp
        ${CORE_DIR}/SampClient.cpp
        ${CORE_DIR}/SampBridge.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cli_test.cpp
)

set(RAKNET_SOURCES
        ${RAKNET_DIR}/BitStream.cpp
        ${RAKNET_DIR}/CheckSum.cpp
        ${RAKNET_DIR}/DataBlockEncryptor.cpp
        ${RAKNET_DIR}/DS_ByteQueue.cpp
        ${RAKNET_DIR}/DS_HuffmanEncodingTree.cpp
        ${RAKNET_DIR}/DS_Table.cpp
        ${RAKNET_DIR}/EncodeClassName.cpp
        ${RAKNET_DIR}/GetTime.cpp
        ${RAKNET_DIR}/InternalPacketPool.cpp
        ${RAKNET_DIR}/NetworkTypes.cpp
        ${RAKNET_DIR}/PluginInterface.cpp
        ${RAKNET_DIR}/RakClient.cpp
        ${RAKNET_DIR}/RakPeer.cpp
        ${RAKNET_DIR}/RakNetStatistics.cpp
        ${RAKNET_DIR}/RakSleep.cpp
        ${RAKNET_DIR}/Rand.cpp
        ${RAKNET_DIR}/ReliabilityLayer.cpp
        ${RAKNET_DIR}/RPCMap.cpp
        ${RAKNET_DIR}/SHA1.cpp
        ${RAKNET_DIR}/SimpleMutex.cpp
        ${RAKNET_DIR}/SocketLayer.cpp
        ${RAKNET_DIR}/StringCompressor.cpp
        ${RAKNET_DIR}/StringTable.cpp
        ${RAKNET_DIR}/TableSerializer.cpp
        ${RAKNET_DIR}/rijndael.cpp
        ${RAKNET_DIR}/SAMP/samp_auth.cpp
        ${RAKNET_DIR}/SAMP/samp_netencr.cpp
)

add_executable(sampcli ${SAMP_SOURCES} ${RAKNET_SOURCES})

add_executable(sampprobe ${CMAKE_CURRENT_SOURCE_DIR}/probe.cpp ${RAKNET_SOURCES})
target_include_directories(sampprobe PRIVATE
        ${CORE_DIR}
        ${CORE_DIR}/compat
        ${RAKNET_DIR}
        ${RAKNET_DIR}/SAMP
)
target_compile_options(sampprobe PRIVATE -w)
if (WIN32)
    target_link_libraries(sampprobe PRIVATE ws2_32 winmm)
endif ()

target_include_directories(sampcli PRIVATE
        ${CORE_DIR}
        ${CORE_DIR}/compat
        ${RAKNET_DIR}
        ${RAKNET_DIR}/SAMP
)

# vendored RakNet predates modern warning cleanliness; keep the log readable
target_compile_options(sampcli PRIVATE -w)

if (WIN32)
    target_link_libraries(sampcli PRIVATE ws2_32 winmm)
endif ()
