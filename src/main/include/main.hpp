#ifndef MAIN_HPP
#define MAIN_HPP

#include <iostream>
#include <vector>

#include "sniffer.hpp"
#include "tests.hpp"
#include "raw_socket.hpp"


class TestSet_Class{
public:

    Tests_Class tests;
    SnifferClass sniffer;

    RawSocket socket;

    std::vector<uint8_t> digital_input;

    TestSet_Class(){
        // Initialization
        this->digital_input.resize(16);

        // Tests
        // this->tests.raw_socket = &socket;
        //Sniffer
        // this->sniffer.socket = &socket;
        this->sniffer.digitalInput = &digital_input;
        this->tests.digital_input = &digital_input;

        
    }


};

#endif // MAIN_HPP