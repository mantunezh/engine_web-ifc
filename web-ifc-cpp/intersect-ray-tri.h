#pragma once

#include "../deps/glm/glm/glm.hpp"

namespace webifc
{

    // https://www.iquilezles.org/www/articles/intersectors/intersectors.htm
    bool intersect_ray_triangle(
        const glm::dvec3& origin, const glm::dvec3& end, const glm::dvec3& A, const glm::dvec3& B, const glm::dvec3& C, glm::dvec3& out, double& t, bool infiniteLength = false
    ) {
        glm::dvec3 dir = end - origin;
        glm::dvec3 E1 = B - A;
        glm::dvec3 E2 = C - A;
        glm::dvec3 ROV0 = origin - A;
        glm::dvec3 N = glm::cross(E1, E2);
        glm::dvec3 Q = glm::cross(ROV0, dir);
        double d = dot(dir, N);
        if (d == 0)
        {
            return false;
        }
        double det = 1.0 / d;
        double u = det * glm::dot(E2, (Q * -1.0));
        double v = det * glm::dot(E1, Q);
        t = det * glm::dot(ROV0, (N * -1.0));

        if (t > 0.9999 && t < 1)
        {
            t = 1;
        }

        if (t < 0.0001 && t > 0)
        {
            t = 0;
        }

        //log(t, u, v, det);
        bool result = (t >= 0.0 - EPS_SMALL && u >= 0.0 - EPS_SMALL && v >= 0.0 - EPS_SMALL && (u + v) <= 1.0 + EPS_SMALL && (t <= 1 + EPS_SMALL || infiniteLength));
        if (result)
        {
            glm::dvec3 vec = origin + (dir * t);
            out = vec;
            return true;
        }
        else
        {
            return false;
        }
    }

    struct TriTriResult
    {
        bool hasIntersection = true;
        glm::dvec3 start;
        glm::dvec3 end;
    };

    TriTriResult intersect_triangle_triangle(
        const glm::dvec3& a, const glm::dvec3& b, const glm::dvec3& c,
        const glm::dvec3& d, const glm::dvec3& e, const glm::dvec3& f)
    {
        glm::dvec3 pab;
        glm::dvec3 pbc;
        glm::dvec3 pca;
        
        glm::dvec3 pde;
        glm::dvec3 pef;
        glm::dvec3 pfd;


        // find all edge intersections
        double t; // ignore
        bool ab = intersect_ray_triangle(a, b, d, e, f, pab, t);
        bool bc = intersect_ray_triangle(b, c, d, e, f, pbc, t);
        bool ca = intersect_ray_triangle(c, a, d, e, f, pca, t);

        bool de = intersect_ray_triangle(d, e, a, b, c, pde, t);
        bool ef = intersect_ray_triangle(e, f, a, b, c, pef, t);
        bool fd = intersect_ray_triangle(f, d, a, b, c, pfd, t);

        // count intersecting edges
        int abc = ab + bc + ca;
        int def = de + ef + fd;

        // no intersect
        if (!abc && !def)
        {
            TriTriResult result;
            result.hasIntersection = false;
            return result;
        }

        if (abc == 3 || def == 3)
        {
            TriTriResult result;
            result.hasIntersection = false;
            return result;
        }

        if (abc == 2)
        {
            // triangle abc inside def
            glm::dvec3 p1 = ab ? pab : pbc;
            glm::dvec3 p2 = ca ? pca : pbc;

            return TriTriResult{
                true,
                p1,
                p2
            };
        }
        else if (def == 2)
        {
            // triangle def inside abc
            glm::dvec3 p1 = de ? pde : pef;
            glm::dvec3 p2 = fd ? pfd : pef;

            return TriTriResult{
                true,
                p1,
                p2
            };
        }
        else
        {
            // TODO: this is shady
            if (abc == 1 && def == 0)
            {
                glm::dvec3 p1 = ab ? pab : bc ? pbc : pca;

                return TriTriResult{
                    true,
                    p1,
                    p1
                };
            }
            else if (abc == 0 && def == 1)
            {
                glm::dvec3 p2 = de ? pde : ef ? pef : pfd;

                return TriTriResult{
                    true,
                    p2,
                    p2
                };
            }
            else
            {
                // both intersect with at least one element
                glm::dvec3 p1 = ab ? pab : bc ? pbc : pca;
                glm::dvec3 p2 = de ? pde : ef ? pef : pfd;

                return TriTriResult{
                    true,
                    p1,
                    p2
                };
            }
        }
    }
}