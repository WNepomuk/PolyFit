/*
Copyright (C) 2017  Liangliang Nan
https://3d.bk.tudelft.nl/liangliang/ - liangliang.nan@gmail.com

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include "../basic/logger.h"
#include "../model/point_set.h"
#include "../model/map.h"
#include "../method/method_global.h"
#include "../method/hypothesis_generator.h"
#include "../method/face_selection.h"
#include "../model/map_io.h"
#include "../model/point_set_io.h"

/* This program expects five command line arguments:
 *
 *      1. Data fitting parameter
 *      2. Model coverage parameter
 *      3. Model complexity parameter
 *      => These 3 should add up to 1!
 *
 *      4. Path to the input file
 *      5. Path to write the output file to
*/
int main(int argc, char **argv)
{
    // Parse command line arguments
    if (argc < 6) {
        std::cout << "To few arguments. Needed 5, got " << argc - 1 << "." << std::endl;
        return EXIT_FAILURE;
    }

    if (argc > 6) {
        std::cout << "To many arguments. Needed 5, got " << argc - 1 << "." << std::endl;
        return EXIT_FAILURE;
    }

    float data_fitting = std::stof(argv[1]);
    float model_coverage = std::stof(argv[2]);
    float model_complexity = std::stof(argv[3]);

    if (data_fitting + model_coverage + model_complexity != 1.0) {
        std::cout << "Arguments should sum up to 1." << std::endl;
        return EXIT_FAILURE;
    }
    
    // initialize the logger (this is not optional)
    Logger::initialize();

    std::string fileName = "/../data/log.txt";

    auto fileLogger = FileLogger(fileName);

    // input point cloud file name
    const std::string input_file = argv[4];
    // output mesh file name
    const std::string output_file = argv[5];

    // below are the default parameters (change these when necessary)
    Method::lambda_data_fitting = data_fitting; // 0.43
    Method::lambda_model_coverage = model_coverage; // 0.27
    Method::lambda_model_complexity = model_complexity; // 0.3

    // load point cloud from file
    PointSet* pset = PointSetIO::read(input_file);
    if (!pset) {
        std::cerr << "failed loading point cloud from file: " << input_file << std::endl;
        return EXIT_FAILURE;
    }

    // step 1: refine planes
    std::cout << "refining planes..." << std::endl;
    const std::vector<VertexGroup::Ptr>& groups = pset->groups();
    if (groups.empty()) {
        std::cerr << "planar segments do not exist" << std::endl;
        return EXIT_FAILURE;
    }
    HypothesisGenerator hypothesis(pset);
    hypothesis.refine_planes();

    // step 2: generate face hypothesis
    std::cout << "generating plane hypothesis..." << std::endl;
    Map* mesh = hypothesis.generate();
    if (!mesh) {
        std::cerr << "failed generating candidate faces. Please check if the input point cloud has good planar segments" << std::endl;
        return EXIT_FAILURE;
    }
    hypothesis.compute_confidences(mesh, false);

    // step 3: face selection
    std::cout << "optimization..." << std::endl;
    const auto& adjacency = hypothesis.extract_adjacency(mesh);
    FaceSelection selector(pset, mesh);
    selector.optimize(adjacency, LinearProgramSolver::SCIP);
    if (mesh->size_of_facets() == 0) {
        std::cerr << "optimization failed: model has on faces" << std::endl;
        return EXIT_FAILURE;
    }
    // now we don't need the point cloud anymore, and it can be deleted
    delete pset;

    // step 4: save result to file
    if (MapIO::save(output_file, mesh))
        std::cout << "reconstructed model saved to file: " << output_file << std::endl;
    else {
        std::cerr << "failed saving reconstructed model to file: " << output_file << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
};

