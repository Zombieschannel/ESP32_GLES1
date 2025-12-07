idf_component_register(SRCS "GLES.cpp" "EGL.cpp" "Context.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES arduino TFT_eSPI SFGAL)
