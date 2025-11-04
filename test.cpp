#include "SuiteSparseDownloader.hpp"
#include <iostream>

int main()
{
    SuiteSparseDownloader downloader;
    SuiteSparseDownloader::MatrixFilter filter;
    filter.minRows = 10;
    filter.maxRows = 50;
    filter.minAverageNonzeroPerRow = 5;
    filter.maxAverageNonzeroPerRow = 10;
    filter.names = {"bcsstk01", "ex5"};
    std::vector<SuiteSparseDownloader::MatrixInfo> matrices = downloader.getMatrices(filter);
    downloader.downloadMatrices("matrices/", matrices);
    for (const auto& matrix: matrices)
    {
        if (!matrix.isValid) continue;
        std::cout
            << "id: "               << matrix.id               << '\n'
            << "groupName: "        << matrix.groupName        << '\n'
            << "name: "             << matrix.name             << '\n'
            << "rows: "             << matrix.rows             << '\n'
            << "cols: "             << matrix.cols             << '\n'
            << "nonzeros: "         << matrix.nonzeros         << '\n'
            << "isReal: "           << (matrix.isReal ? "true" : "false") << '\n'
            << "isBinary: "         << (matrix.isBinary ? "true" : "false") << '\n'
            << "is2d3d: "           << (matrix.is2d3d ? "true" : "false") << '\n'
            << "isPosDef: "         << (matrix.isPosDef ? "true" : "false") << '\n'
            << "patternSymmetry: "  << (matrix.patternSymmetry ? "true" : "false")  << '\n'
            << "numericSymmetry: "  << (matrix.numericSymmetry ? "true" : "false")  << '\n'
            << "kind: "             << matrix.kind             << '\n'
            << "downloadLink: "     << matrix.downloadLink     << '\n'
            << "installationPath: " << matrix.installationPath << '\n'
            << "----------------------------------------" << std::endl;
    }

    return 0;
}
