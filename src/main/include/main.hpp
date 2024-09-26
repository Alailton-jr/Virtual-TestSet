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

    TestSet_Class(){
        this->tests.raw_socket = &socket;
        this->sniffer.socket = &socket;
    }


};

#endif // MAIN_HPP