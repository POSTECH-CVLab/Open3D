// ----------------------------------------------------------------------------
// -                        Open3D: www.open3d.org                            -
// ----------------------------------------------------------------------------
// The MIT License (MIT)
//
// Copyright (c) 2018 www.open3d.org
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
// ----------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4267)
#endif

#include "Open3D/Geometry/KnnFaiss.h"

#include <faiss/IndexFlat.h>

#include "Open3D/Geometry/HalfEdgeTriangleMesh.h"
#include "Open3D/Geometry/PointCloud.h"
#include "Open3D/Geometry/TriangleMesh.h"
#include "Open3D/Utility/Console.h"

namespace open3d {
namespace geometry {

KnnFaiss::KnnFaiss() {}

KnnFaiss::KnnFaiss(const Eigen::MatrixXd &data) { SetMatrixData(data); }

KnnFaiss::KnnFaiss(const Geometry &geometry) { SetGeometry(geometry); }

KnnFaiss::KnnFaiss(const registration::Feature &feature) {
    SetFeature(feature);
}

KnnFaiss::~KnnFaiss() {}

bool KnnFaiss::SetMatrixData(const Eigen::MatrixXd &data) {
    return SetRawData(Eigen::Map<const Eigen::MatrixXd>(
            data.data(), data.rows(), data.cols()));
}

bool KnnFaiss::SetGeometry(const Geometry &geometry) {
    switch (geometry.GetGeometryType()) {
        case Geometry::GeometryType::PointCloud:
            return SetRawData(Eigen::Map<const Eigen::MatrixXd>(
                    (const double *)((const PointCloud &)geometry)
                            .points_.data(),
                    3, ((const PointCloud &)geometry).points_.size()));
        case Geometry::GeometryType::TriangleMesh:
        case Geometry::GeometryType::HalfEdgeTriangleMesh:
            return SetRawData(Eigen::Map<const Eigen::MatrixXd>(
                    (const double *)((const TriangleMesh &)geometry)
                            .vertices_.data(),
                    3, ((const TriangleMesh &)geometry).vertices_.size()));
        case Geometry::GeometryType::Image:
        case Geometry::GeometryType::Unspecified:
        default:
            utility::LogWarning(
                    "[KnnFaiss::SetGeometry] Unsupported Geometry type.");
            return false;
    }
}

bool KnnFaiss::SetFeature(const registration::Feature &feature) {
    return SetMatrixData(feature.data_);
}

template <typename T>
int KnnFaiss::Search(const T &query,
                        const KDTreeSearchParam &param,
                        std::vector<int> &indices,
                        std::vector<float> &distance2) const {
    switch (param.GetSearchType()) {
        case KDTreeSearchParam::SearchType::Knn:
            return SearchKNN(query, ((const KDTreeSearchParamKNN &)param).knn_,
                             indices, distance2);
        case KDTreeSearchParam::SearchType::Radius:
            return SearchRadius(
                    query, ((const KDTreeSearchParamRadius &)param).radius_,
                    indices, distance2);
        case KDTreeSearchParam::SearchType::Hybrid:
        default:
            return -1;
    }
    return -1;
}

template <typename T>
int KnnFaiss::SearchKNN(const T &query,
                           int knn,
                           std::vector<int> &indices,
                           std::vector<float> &distance2) const {
    // This is optimized code for heavily repeated search.
    // Other flann::Index::knnSearch() implementations lose performance due to
    // memory allocation/deallocation.
    if (data_.empty() || dataset_size_ <= 0 ||
        size_t(query.rows()) != dimension_ || knn < 0) {
        return -1;
    }

    index->search(query.cols(), query.data(), knn, distance2.data(), indices.data());
    return knn;
}

template <typename T>
int KnnFaiss::SearchRadius(const T &query,
                              float radius,
                              std::vector<int> &indices,
                              std::vector<float> &distance2) const {
    // This is optimized code for heavily repeated search.
    // Since max_nn is not given, we let flann to do its own memory management.
    // Other flann::Index::radiusSearch() implementations lose performance due
    // to memory management and CPU caching.
    if (data_.empty() || dataset_size_ <= 0 ||
        size_t(query.rows()) != dimension_) {
        return -1;
    }
    faiss::RangeSearchResult result(query.cols());
    result.labels = indices.data();
    result.distances = distance2.data();
    index->range_search(query.cols(), query.data(), radius, &result);
    return 1;
}

bool KnnFaiss::SetRawData(const Eigen::Map<const Eigen::MatrixXd> &data) {
    dimension_ = data.rows();
    dataset_size_ = data.cols();
    if (dimension_ == 0 || dataset_size_ == 0) {
        utility::LogWarning("[KnnFaiss::SetRawData] Failed due to no data.");
        return false;
    }
    data_.resize(dataset_size_ * dimension_);
    memcpy(data_.data(), data.data(),
           dataset_size_ * dimension_ * sizeof(float));
    index.reset(new faiss::indexFlatL2(dimension));
    index->add(dataset_size_, data_.data())
    return true;
}

template int KnnFaiss::Search<Eigen::Vector3d>(
        const Eigen::Vector3d &query,
        const KDTreeSearchParam &param,
        std::vector<int> &indices,
        std::vector<float> &distance2) const;
template int KnnFaiss::SearchKNN<Eigen::Vector3d>(
        const Eigen::Vector3d &query,
        int knn,
        std::vector<int> &indices,
        std::vector<float> &distance2) const;
template int KnnFaiss::SearchRadius<Eigen::Vector3d>(
        const Eigen::Vector3d &query,
        float radius,
        std::vector<int> &indices,
        std::vector<float> &distance2) const;

template int KnnFaiss::Search<Eigen::VectorXd>(
        const Eigen::VectorXd &query,
        const KDTreeSearchParam &param,
        std::vector<int> &indices,
        std::vector<float> &distance2) const;
template int KnnFaiss::SearchKNN<Eigen::VectorXd>(
        const Eigen::VectorXd &query,
        int knn,
        std::vector<int> &indices,
        std::vector<float> &distance2) const;
template int KnnFaiss::SearchRadius<Eigen::VectorXd>(
        const Eigen::VectorXd &query,
        float radius,
        std::vector<int> &indices,
        std::vector<float> &distance2) const;

}  // namespace geometry
}  // namespace open3d

#ifdef _MSC_VER
#pragma warning(pop)
#endif