set(TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(test_brainfuck)
# add_subdirectory(network)
# add_subdirectory(neuron)

add_custom_target(runtests
    COMMAND ${CMAKE_CTEST_COMMAND}
    DEPENDS
        test_brainfuck_cpu
    #     neuron_test_output
    #     neuron_test_mutation
    #     neuron_test_rollback
    #     neuron_test_save_restore
    #     save_restore_test
    #     # micronet_test
    #     # micronet_test_save_restore
    #     network_instantiate_test
    #     network_test_micronet
    #     network_test_save_restore
)
