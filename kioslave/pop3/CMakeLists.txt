

set(pop3_optional_includes)
set(pop3_optional_libs)

if(SASL2_FOUND)
   set(pop3_optional_includes ${pop3_optional_includes} ${SASL2_INCLUDE_DIR})
   set(pop3_optional_libs     ${pop3_optional_libs}     ${SASL2_LIBRARIES})
   set(HAVE_LIBSASL2 1)
else()
   set(HAVE_LIBSASL2 0)
endif()

configure_file(pop3-config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/pop3-config.h )

include_directories(  ${pop3_optional_includes}  )

########### next target ###############

set(kio_pop3_PART_SRCS pop3.cpp )


kde4_add_plugin(kio_pop3 ${kio_pop3_PART_SRCS})


target_link_libraries(kio_pop3  ${KDE4_KIO_LIBS} ${pop3_optional_libs})

install(TARGETS kio_pop3  DESTINATION ${PLUGIN_INSTALL_DIR} )


########### install files ###############

install( FILES pop3.protocol pop3s.protocol  DESTINATION  ${SERVICES_INSTALL_DIR} )



