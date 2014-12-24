
struct d3dsurface_t {
    std::shared_ptr<ID3D11Buffer> m_vertexBuffer;
    std::shared_ptr<ID3D11Buffer> m_indexBuffer;

    int m_numIndices;
    material_t m_material;
};

typedef struct d3dsurface_t gsurface_t;
