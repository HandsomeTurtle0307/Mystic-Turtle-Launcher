/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 HandsomeTurtle0307
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License.
 */

#include "SampleData.h"

namespace SampleData {

const QVector<VersionInfo> &localVersions()
{
    static const QVector<VersionInfo> versions = {
        VersionInfo{
            QStringLiteral("1.21.4"),
            QStringLiteral("Fabric"),
            QStringLiteral("0.16.10"),
            QStringLiteral("今天 14:32"),
            842,
            QColor(QStringLiteral("#89A8FF")),
            QVector<ModInfo>{
                {QStringLiteral("Fabric API"), QStringLiteral("0.119.4+1.21.4"),
                 QStringLiteral("Fabric 模组生态的核心前置与事件框架。"),
                 QStringLiteral("核心"), true},
                {QStringLiteral("Sodium"), QStringLiteral("0.6.9"),
                 QStringLiteral("重写渲染管线，大幅提升帧率并减少卡顿。"),
                 QStringLiteral("性能"), true},
                {QStringLiteral("Iris Shaders"), QStringLiteral("1.8.8"),
                 QStringLiteral("在 Fabric 环境下加载 OptiFine 光影包。"),
                 QStringLiteral("光影"), true},
                {QStringLiteral("Lithium"), QStringLiteral("0.14.3"),
                 QStringLiteral("优化 AI、物理和区块逻辑，让世界运转更轻快。"),
                 QStringLiteral("性能"), true},
                {QStringLiteral("Xaero's Minimap"), QStringLiteral("25.2.0"),
                 QStringLiteral("轻量、清晰的实时小地图与路径点系统。"),
                 QStringLiteral("实用"), true},
                {QStringLiteral("Mod Menu"), QStringLiteral("13.0.3"),
                 QStringLiteral("在游戏内查看并配置已安装的 Fabric 模组。"),
                 QStringLiteral("界面"), true},
                {QStringLiteral("Cloth Config API"), QStringLiteral("17.0.144"),
                 QStringLiteral("为众多模组提供统一的设置界面组件。"),
                 QStringLiteral("前置"), true},
                {QStringLiteral("AppleSkin"), QStringLiteral("3.0.6"),
                 QStringLiteral("显示隐藏的饥饿值、饱和度和食物收益。"),
                 QStringLiteral("实用"), true},
                {QStringLiteral("Zoomify"), QStringLiteral("2.14.2"),
                 QStringLiteral("顺滑的变焦视野，可通过滚轮调整倍率。"),
                 QStringLiteral("实用"), true},
                {QStringLiteral("Continuity"), QStringLiteral("3.0.0"),
                 QStringLiteral("连接纹理与自发光材质支持，已达到稳定状态。"),
                 QStringLiteral("视觉"), false},
            },
        },
        VersionInfo{
            QStringLiteral("1.20.1"),
            QStringLiteral("Forge"),
            QStringLiteral("47.3.0"),
            QStringLiteral("昨天 22:18"),
            1264,
            QColor(QStringLiteral("#FFC66D")),
            QVector<ModInfo>{
                {QStringLiteral("Just Enough Items"), QStringLiteral("15.20.0.106"),
                 QStringLiteral("快速查询物品用途与合成配方。"),
                 QStringLiteral("实用"), true},
                {QStringLiteral("JourneyMap"), QStringLiteral("5.10.1"),
                 QStringLiteral("实时地图、路径点与服务器地图同步。"),
                 QStringLiteral("实用"), true},
                {QStringLiteral("Create"), QStringLiteral("0.5.1.j"),
                 QStringLiteral("用旋转动力和齿轮构建你的自动化机械。"),
                 QStringLiteral("科技"), true},
                {QStringLiteral("Embeddium"), QStringLiteral("0.3.31"),
                 QStringLiteral("面向 Forge 的 Sodium 移植版渲染优化。"),
                 QStringLiteral("性能"), true},
                {QStringLiteral("Oculus"), QStringLiteral("1.7.0"),
                 QStringLiteral("在 Forge 上加载光影包。"),
                 QStringLiteral("光影"), true},
                {QStringLiteral("Cloth Config API"), QStringLiteral("11.1.136"),
                 QStringLiteral("模组设置界面的通用前置。"),
                 QStringLiteral("前置"), true},
                {QStringLiteral("AppleSkin"), QStringLiteral("2.5.1"),
                 QStringLiteral("显示饥饿相关的隐藏数值。"),
                 QStringLiteral("实用"), true},
            },
        },
        VersionInfo{
            QStringLiteral("1.21.1"),
            QStringLiteral("原版"),
            QStringLiteral("官方"),
            QStringLiteral("10 月 24 日"),
            326,
            QColor(QStringLiteral("#6EE7B7")),
            QVector<ModInfo>{},
        },
    };
    return versions;
}

const QVector<GameRelease> &gameReleases()
{
    static const QVector<GameRelease> releases = {
        GameRelease{QStringLiteral("1.21.4"), QStringLiteral("正式版"), QStringLiteral("2024-12-03"),
                    QStringLiteral("The Garden Awakens"), true, QColor(QStringLiteral("#6EE7B7"))},
        GameRelease{QStringLiteral("1.21.1"), QStringLiteral("正式版"), QStringLiteral("2024-08-08"),
                    QStringLiteral("Bundles of Bravery"), false, QColor(QStringLiteral("#89A8FF"))},
        GameRelease{QStringLiteral("1.20.6"), QStringLiteral("正式版"), QStringLiteral("2024-04-29"),
                    QStringLiteral("Armored Paws"), false, QColor(QStringLiteral("#FFC66D"))},
        GameRelease{QStringLiteral("1.21.4-pre1"), QStringLiteral("快照"), QStringLiteral("2024-11-20"),
                    QStringLiteral("Winter Drop 预览"), false, QColor(QStringLiteral("#B7A0FF"))},
        GameRelease{QStringLiteral("1.20.1"), QStringLiteral("旧版本"), QStringLiteral("2023-06-12"),
                    QStringLiteral("Trails & Tales"), false, QColor(QStringLiteral("#FFC66D"))},
        GameRelease{QStringLiteral("1.18.2"), QStringLiteral("旧版本"), QStringLiteral("2022-02-28"),
                    QStringLiteral("Caves & Cliffs"), false, QColor(QStringLiteral("#6EE7B7"))},
    };
    return releases;
}

const QVector<CatalogMod> &catalogMods()
{
    static const QVector<CatalogMod> mods = {
        CatalogMod{QStringLiteral("Sodium"), QStringLiteral("jellysquid3"),
                   QStringLiteral("重写渲染管线，让帧率更稳定、视野更顺滑。"),
                   QStringLiteral("性能"), QStringLiteral("1.2M 下载"), QColor(QStringLiteral("#7AE5C0"))},
        CatalogMod{QStringLiteral("Iris Shaders"), QStringLiteral("IrisShaders"),
                   QStringLiteral("在 Fabric 上运行喜爱的光影包，兼容性出色。"),
                   QStringLiteral("光影"), QStringLiteral("860K 下载"), QColor(QStringLiteral("#89A8FF"))},
        CatalogMod{QStringLiteral("Create"), QStringLiteral("simibubi"),
                   QStringLiteral("用旋转动力、齿轮与机械臂搭建你的工业世界。"),
                   QStringLiteral("科技"), QStringLiteral("740K 下载"), QColor(QStringLiteral("#FFC66D"))},
        CatalogMod{QStringLiteral("JEI"), QStringLiteral("mezz"),
                   QStringLiteral("在游戏内快速查询物品用途与合成配方。"),
                   QStringLiteral("实用"), QStringLiteral("2.8M 下载"), QColor(QStringLiteral("#B7A0FF"))},
        CatalogMod{QStringLiteral("Xaero's Minimap"), QStringLiteral("x-aero"),
                   QStringLiteral("轻量又完整的实时小地图，支持路径点与传送。"),
                   QStringLiteral("实用"), QStringLiteral("1.9M 下载"), QColor(QStringLiteral("#6EE7B7"))},
        CatalogMod{QStringLiteral("Fabric API"), QStringLiteral("FabricMC"),
                   QStringLiteral("大多数 Fabric 模组需要的核心依赖与事件框架。"),
                   QStringLiteral("核心"), QStringLiteral("3.4M 下载"), QColor(QStringLiteral("#89A8FF"))},
        CatalogMod{QStringLiteral("AppleSkin"), QStringLiteral("squeek502"),
                   QStringLiteral("显示饥饿值、饱和度以及食物带来的隐藏收益。"),
                   QStringLiteral("实用"), QStringLiteral("1.1M 下载"), QColor(QStringLiteral("#FFC66D"))},
        CatalogMod{QStringLiteral("Litematica"), QStringLiteral("masa"),
                   QStringLiteral("以全息投影辅助大型建筑，让蓝图落地更轻松。"),
                   QStringLiteral("建造"), QStringLiteral("520K 下载"), QColor(QStringLiteral("#B7A0FF"))},
    };
    return mods;
}

int enabledModCount(const VersionInfo &version)
{
    int count = 0;
    for (const ModInfo &mod : version.mods) {
        if (mod.enabled) {
            ++count;
        }
    }
    return count;
}

} // namespace SampleData
