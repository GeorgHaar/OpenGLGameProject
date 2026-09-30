#ifndef SKELETON_H
#define SKELETON_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <assimp/scene.h>

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using Pose = std::unordered_map<std::string, glm::mat4>;

inline void AddToPose(Pose &pose, const std::string &bone, const glm::mat4 &transform) {
    auto found = pose.find(bone);
    if (found == pose.end())
        pose.emplace(bone, transform);
    else
        found->second = found->second * transform;
}

inline float Glatt(float t) {
    return t * t * (3.0f - 2.0f * t);
}

inline glm::mat4 DrehungX(float grad) {
    return glm::rotate(glm::mat4(1.0f), glm::radians(grad), glm::vec3(1.0f, 0.0f, 0.0f));
}
inline glm::mat4 DrehungY(float grad) {
    return glm::rotate(glm::mat4(1.0f), glm::radians(grad), glm::vec3(0.0f, 1.0f, 0.0f));
}
inline glm::mat4 DrehungZ(float grad) {
    return glm::rotate(glm::mat4(1.0f), glm::radians(grad), glm::vec3(0.0f, 0.0f, 1.0f));
}

class Skeleton {
public:
    static constexpr int MAX_BONES = 100;

    void Load(const aiScene *scene) {
        copyNode(scene->mRootNode, root);
    }

    int RegisterBone(const aiBone *bone) {
        std::string name = bone->mName.C_Str();

        auto found = bones.find(name);
        if (found != bones.end())
            return found->second.id;

        BoneInfo info;
        info.id = static_cast<int>(bones.size());
        info.offset = toGlm(bone->mOffsetMatrix);

        if (info.id >= MAX_BONES)
            std::cout << "WARNUNG: mehr als " << MAX_BONES << " Knochen, "
                      << name << " wird nicht animiert" << std::endl;

        bones.emplace(name, info);
        return info.id;
    }

    bool HasBones() const {
        return !bones.empty();
    }

    void ComputeBoneMatrices(const Pose &pose, std::vector<glm::mat4> &out) const {
        size_t anzahl = bones.size() < static_cast<size_t>(MAX_BONES) ? bones.size() : MAX_BONES;
        out.assign(anzahl, glm::mat4(1.0f));
        walk(root, glm::mat4(1.0f), pose, out);
    }

private:
    struct BoneInfo {
        int id;
        glm::mat4 offset;
    };

    struct Node {
        std::string name;
        glm::mat4 transform;
        std::vector<Node> children;
    };

    Node root;
    std::unordered_map<std::string, BoneInfo> bones;

    void copyNode(const aiNode *src, Node &dst) {
        dst.name = src->mName.C_Str();
        dst.transform = toGlm(src->mTransformation);
        dst.children.resize(src->mNumChildren);
        for (unsigned int i = 0; i < src->mNumChildren; i++)
            copyNode(src->mChildren[i], dst.children[i]);
    }

    void walk(const Node &node, const glm::mat4 &parent, const Pose &pose, std::vector<glm::mat4> &out) const {
        glm::mat4 global = parent * node.transform;

        auto extra = pose.find(node.name);
        if (extra != pose.end())
            global = global * extra->second;

        auto bone = bones.find(node.name);
        if (bone != bones.end() && bone->second.id < static_cast<int>(out.size()))
            out[bone->second.id] = global * bone->second.offset;

        for (const Node &child : node.children)
            walk(child, global, pose, out);
    }

    static glm::mat4 toGlm(const aiMatrix4x4 &m) {
        return glm::transpose(glm::make_mat4(&m.a1));
    }
};

#endif
