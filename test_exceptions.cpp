#include "notepad_exception.h"
#include <iostream>

void test_single_catch()
{
    try {
        throw file_not_found_exception("missing.txt");
    } catch (const notepad_exception& notepad_error) {
        std::cout << "Caught notepad_exception: " << notepad_error.what() << "\n";
    }
}

void test_multiple_catches()
{
    try {
        throw file_not_found_exception("missing.txt");
    } catch (const file_not_found_exception& not_found_err) {
        std::cout << "Test 2a: Caught file_not_found_exception: " << not_found_err.what() << "\n";
    } catch (const notepad_exception& notepad_err) {
        std::cout << "Test 2a: Caught notepad_exception: " << notepad_err.what() << "\n";
    } catch (const std::exception& standard_exception) {
        std::cout << "Test 2a: Caught std::exception: " << standard_exception.what() << "\n";
    }

    try {
        throw file_read_exception("corrupt.dat");
    } catch (const file_not_found_exception& not_found_err) {
        std::cout << "Test 2b: Caught file_not_found_exception: " << not_found_err.what() << "\n";
    } catch (const notepad_exception& notepad_err) {
        std::cout << "Test 2b: Caught notepad_exception: " << notepad_err.what() << "\n";
    } catch (const std::exception& standard_exception) {
        std::cout << "Test 2b: Caught std::exception: " << standard_exception.what() << "\n";
    }

    try {
        throw std::runtime_error("Unknown error");
    } catch (const file_not_found_exception& not_found_err) {
        std::cout << "Test 2c: Caught file_not_found_exception: " << not_found_err.what() << "\n";
    } catch (const notepad_exception& notepad_err) {
        std::cout << "Test 2c: Caught notepad_exception: " << notepad_err.what() << "\n";
    } catch (const std::exception& standard_exception) {
        std::cout << "Test 2c: Caught std::exception: " << standard_exception.what() << "\n";
    }
}

void open_file_inner(const std::string& target_filename)
{
    try {
        throw file_not_found_exception(target_filename);
    } catch (const notepad_exception& internal_exception) {
        std::cout << "Inner catch in open_file: " << internal_exception.what() << "\nRethrowing...\n";
        throw;
    }
}

void test_rethrow()
{
    try {
        open_file_inner("missing.txt");
    } catch (const notepad_exception& caught_exception) {
        std::cout << "Outer catch in main: " << caught_exception.what() << "\n";
    }
}

int main()
{
    std::cout << "Test 1: Single catch\n";
    test_single_catch();

    std::cout << "\nTest 2: Multiple catches\n";
    test_multiple_catches();

    std::cout << "\nTest 3: Rethrow\n";
    test_rethrow();

    return 0;
}