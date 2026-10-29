# cs_check.py -- a numerical check of the Cartwright-Steger matrices (needs numpy).
# Copyright Danny Calegari 2026, released under the terms of the GNU GPL.
import numpy as np
z = np.exp(1j*np.pi/6); r = np.sqrt(3.0)
Fp = np.array([[r+1,-1,0],[-1,r-1,0],[0,0,-1]],dtype=complex)
F  = np.diag([1,1,1-r]).astype(complex)
g0 = np.array([[1,0,0],[1,1-r,0],[0,0,1]],dtype=complex)
up = np.array([[z**3+z**2-z, 1-z, 0],[z**3+z**2-1, z-z**3, 0],[0,0,1]])
vp = np.array([[z**3,0,0],[z**3+z**2-z-1,1,0],[0,0,1]])
bp = np.array([[1,0,0],[-2*z**3-z**2+2*z+2, z**3+z**2-z-1, -z**3-z**2],[z**2+z, -z**3-1, -z**3+z+1]])
inv = np.linalg.inv
print("F' = (r-1)^-1 g0* F g0 :", np.abs(Fp - g0.conj().T@F@g0/(r-1)).max())
for nm,M in [("u'",up),("v'",vp),("b'",bp)]:
    print(nm,"unitary wrt F':", np.abs(M.conj().T@Fp@M - Fp).max())
u = g0@up@inv(g0); v = g0@vp@inv(g0); b = g0@bp@inv(g0)
def scal(M):  # distance of M from the scalar matrices
    c = np.trace(M)/3; return np.abs(M - c*np.eye(3)).max()
mp = np.linalg.matrix_power
I = inv
print("u^3",scal(mp(u,3)),"v^4",scal(mp(v,4)),"b^3",scal(mp(b,3)))
print("(uv)^2(vu)^-2",scal(mp(u@v,2)@I(mp(v@u,2))),"vb(bv)^-1",scal(v@b@I(b@v)))
print("(buv)^3",scal(mp(b@u@v,3)),"(buvu)^2 v",scal(mp(b@u@v@u,2)@v))
j = mp(u@v,2); jn = j/j[2,2]
print("j=(uv)^2 normalised:", np.round(jn,12).diagonal(), " zeta =", z)
J = lambda k: mp(j,k)
a1 = v@u@I(v)@J(4)@b@u@v@J(2)
a2 = mp(v,2)@u@b@u@I(v)@u@mp(v,2)@j
a3 = I(u)@mp(v,2)@u@J(9)@b@I(v)@u@I(v)@J(8)
for nm,M in [("a1",a1),("a2",a2),("a3",a3)]:
    print(nm,"unitary wrt F:", np.abs(M.conj().T@F@M - F).max())
J4 = J(4)
print("j4 a2 j-4 = a3^-1 :", scal(J4@a2@I(J4)@a3))
print("j4 a1 j-4 = a3 a2^-3 a3^3 a1 :", scal(J4@a1@I(J4)@I(a3@mp(I(a2),3)@mp(a3,3)@a1)))
rhs3 = I(a1)@I(a2)@a1@mp(a2,2)@I(a1)@I(a2)@a1@I(a3)@I(a1)@a2@a1
print("j4 a3 j-4 = (long word) :", scal(J4@a3@I(J4)@I(rhs3)))
def goldman(M):
    t = np.trace(M)/np.linalg.det(M)**(1/3)
    # det^(1/3) branch ambiguity multiplies t by a cube root of unity; f is invariant under that
    return abs(t)**4 - 8*(t**3).real + 18*abs(t)**2 - 27
for nm,M in [("a1",a1),("a2",a2),("a3",a3),("a1a2^-1a3^2",a1@I(a2)@mp(a3,2))]:
    print(nm,"Goldman discriminant (>0 loxodromic):", round(goldman(M),6))
