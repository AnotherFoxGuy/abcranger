/**
 * @brief Partial Least Squares Regression with Eigen
 * 
 * Same source as LDA, \cite friedman2001elements
 * 
 * @file pls-eigen.hpp
 * @author François-David Collin <Francois-David.Collin@umontpellier.fr>
 * @brief 
 * @version 0.1
 * @date 2018-11-08
 * 
 * @copyright Copyright (c) 2018
 * 
 */
#pragma once

#include <Eigen/Dense>

using namespace Eigen;
using namespace std;

template<class Derived, class OtherDerived>
VectorXd pls(const MatrixBase<Derived>& x,
         const MatrixBase<OtherDerived>& y,
         size_t ncomp,
         double threshold,
         MatrixXd& Pls)
{
    auto n = x.rows();
    auto p = x.cols();
    auto r = ncomp;
    RowVectorXd mean = x.colwise().mean();
    RowVectorXd std = ((x.rowwise() - mean).array().square().colwise().sum() / (x.rows() - 1)).sqrt();;
    MatrixXd X = (x.rowwise() - mean).array().rowwise() / std.array();
    MatrixXd Y = MatrixXd::Zero(n, ncomp + 1);
    MatrixXd Z(n,ncomp);
    // Y.col(0) = y.rowwise() - y.colwise().mean();
    double ymean = y.mean();
    Y.col(0).array() = ymean;
    double SSTO = (y.array() - ymean).array().square().sum();
    for (auto m = 0; m < ncomp; m++)
    {
        VectorXd Zm = X * X.transpose() * y;
        double Znorm = Zm.array().square().sum();
        Z.col(m) = Zm;
        double Thetam = Zm.dot(y) / Znorm;
        Y.col(m + 1) = Y.col(m) + Thetam * Zm;
        if ((Y.col(m+1).array() - ymean).array().square().sum() / SSTO >= threshold) {
            r = m - 1;
            break;
        }
        X -= Zm.rowwise().replicate(p) * ((Zm/Znorm).transpose() * X).asDiagonal();
    }
    Pls = Z.block(0,0,n,r + 1);
    VectorXd res = (Y.block(0,1,n,r + 1).array() - ymean).array().square().colwise().sum() / SSTO;
    return res;
}