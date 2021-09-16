#pragma once

#include <mpc/Common.hpp>

namespace mpc {

template <
    int Tnx, int Tnu, int Tny,
    int Tph, int Tch,
    int Tineq, int Teq>
class Mapping : public Common<Tnx, Tnu, 0, Tny, Tph, Tch, Tineq, Teq> {
private:
    using Common<Tnx, Tnu, 0, Tny, Tph, Tch, Tineq, Teq>::checkOrQuit;
    using Common<Tnx, Tnu, 0, Tny, Tph, Tch, Tineq, Teq>::dim;

public:
    Mapping()
        : Common<Tnx, Tnu, 0, Tny, Tph, Tch, Tineq, Teq>()
    {
    }

    void onInit()
    {
        Iz2uMat.resize((dim.ph.num() * dim.nu.num()), (dim.nu.num() * dim.ch.num()));
        Iu2zMat.resize((dim.nu.num() * dim.ch.num()), (dim.ph.num() * dim.nu.num()));
        Sz2uMat.resize(dim.nu.num(), dim.nu.num());
        Su2zMat.resize(dim.nu.num(), dim.nu.num());

        input_scaling.resize(dim.nu.num());
        state_scaling.resize(dim.nx.num());
        inverse_state_scaling.resize(dim.nx.num());

        input_scaling.setOnes();
        state_scaling.setOnes();
        inverse_state_scaling.setOnes();

        computeMapping();
    }

    void setInputScaling(const cvec<dim.nu> scaling)
    {
        input_scaling = scaling;
        computeMapping();
    }

    void setStateScaling(const cvec<Tnx> scaling)
    {
        state_scaling = scaling;
        inverse_state_scaling = scaling.cwiseInverse();
    }

    mat<(dim.ph * dim.nu), (dim.nu * dim.ch)> Iz2u()
    {
        checkOrQuit();
        return Iz2uMat;
    }

    mat<(dim.nu * dim.ch), (dim.ph * dim.nu)> Iu2z()
    {
        checkOrQuit();
        return Iu2zMat;
    }

    mat<dim.nu, dim.nu> Sz2u()
    {
        checkOrQuit();
        return Sz2uMat;
    }

    mat<dim.nu, dim.nu> Su2z()
    {
        checkOrQuit();
        return Su2zMat;
    }

    cvec<Tnx> StateScaling()
    {
        checkOrQuit();
        return state_scaling;
    }

    cvec<Tnx> StateInverseScaling()
    {
        checkOrQuit();
        return inverse_state_scaling;
    }

    cvec<dim.nu> InputScaling()
    {
        checkOrQuit();
        return input_scaling;
    }

    void unwrapVector(
        const cvec<((dim.ph * dim.nx) + (dim.nu * dim.ch) + Dim<1>())> x,
        cvec<Tnx> x0,
        mat<(dim.ph + Dim<1>()), Tnx>& Xmat,
        mat<(dim.ph + Dim<1>()), dim.nu>& Umat,
        double& slack)
    {
        checkOrQuit();

        cvec<(dim.nu * dim.ch)> u_vec;
        u_vec = x.middleRows((dim.ph.num() * dim.nx.num()), (dim.nu.num() * dim.ch.num()));

        mat<(dim.ph + Dim<1>()), dim.nu> Umv;
        Umv.resize((dim.ph.num() + 1), dim.nu.num());

        cvec<(dim.ph * dim.nu)> tmp_mult;
        tmp_mult = Iz2uMat * u_vec;
        mat<dim.nu, dim.ph> tmp_mapped;
        tmp_mapped = Eigen::Map<mat<dim.nu, dim.ph>>(tmp_mult.data(), dim.nu.num(), dim.ph.num());

        Umv.setZero();
        Umv.middleRows(0, dim.ph.num()) = tmp_mapped.transpose();
        Umv.row(dim.ph.num()) = Umv.row(dim.ph.num() - 1);

        Xmat.setZero();
        Xmat.row(0) = x0.transpose();
        for (size_t i = 1; i < (dim.ph.num() + 1); i++) {
            Xmat.row(i) = x.middleRows(((i - 1) * dim.nx.num()), dim.nx.num()).transpose();
        }

        for (int i = 0; i < Xmat.cols(); i++) {
            Xmat.col(i) /= 1.0 / state_scaling(i);
        }

        // TODO add disturbaces manipulated vars
        Umat.setZero();
        Umat.block(0, 0, dim.ph.num() + 1, dim.nu.num()) = Umv;

        slack = x(x.size() - 1);
    }

protected:
    cvec<dim.nu> input_scaling;
    cvec<Tnx> state_scaling, inverse_state_scaling;

private:
    void computeMapping()
    {
        static cvec<dim.ch> m;
        m.resize(dim.ch.num());

        for (size_t i = 0; i < dim.ch.num(); i++) {
            m(i) = 1;
        }

        m(dim.ch.num() - 1) = dim.ph.num() - dim.ch.num() + 1;

        Iz2uMat.setZero();
        Iu2zMat = Iz2uMat.transpose();

        Sz2uMat.setZero();
        Su2zMat.setZero();
        for (int i = 0; i < Sz2uMat.rows(); ++i) {
            Sz2uMat(i, i) = input_scaling(i);
            Su2zMat(i, i) = 1.0 / input_scaling(i);
        }

        // TODO implement linear interpolation
        int ix = 0;
        int jx = 0;
        for (size_t i = 0; i < dim.ch.num(); i++) {
            Iu2zMat.block(ix, jx, dim.nu.num(), dim.nu.num()) = Su2zMat;
            for (int j = 0; j < m[i]; j++) {
                Iz2uMat.block(jx, ix, dim.nu.num(), dim.nu.num()) = Sz2uMat;
                jx += dim.nu.num();
            }
            ix += dim.nu.num();
        }
    }

    mat<(dim.ph * dim.nu), (dim.nu * dim.ch)> Iz2uMat;
    mat<(dim.nu * dim.ch), (dim.ph * dim.nu)> Iu2zMat;
    mat<dim.nu, dim.nu> Sz2uMat;
    mat<dim.nu, dim.nu> Su2zMat;
};
} // namespace mpc