/*
Skeleton code for storage and buffer management
*/

#include <string>
#include <ios>
#include <fstream>
#include <vector>
#include <string>
#include <string.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include "classes.h"
using namespace std;


int main(int argc, char* const argv[]) {

    // Create the EmployeeRelation file from Employee.csv
    StorageBufferManager manager("EmployeeRelation");
    manager.createFromFile("Employee.csv");

    while (true) { // Infinite loop
        std::string input;
        std::cout << "Enter Employee ID to search (or type 'quit' to exit): ";
        
        // Use getline to read the whole line as input can be numbers or 'quit'
        std::getline(std::cin, input);

        // Check if the user wants to quit
        if (input == "quit") {
            break; // Exit the loop
        }

        // Convert input string to integer for the ID
        int searchId = std::stoi(input);

        // Call the function to search for and print the employee information
        manager.findRecordById(searchId);
    }

    return 0;

}
