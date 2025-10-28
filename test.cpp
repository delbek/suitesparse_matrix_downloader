#include "SuiteSparseDownloader.hpp"

int main()
{
    SuiteSparseDownloader downloader;
    SuiteSparseDownloader::MatrixFilter filter;
    filter.minRows = 10;
    filter.maxRows = 50;
    filter.minAverageNonzeroPerRow = 5;
    filter.maxAverageNonzeroPerRow = 10;
    auto matrices = downloader.getMatrices(filter);
    downloader.downloadMatrices("matrices/", matrices);

    return 0;
}
