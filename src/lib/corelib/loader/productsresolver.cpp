/****************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qbs.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "productsresolver.h"

#include "itemreader.h"
#include "loaderutils.h"
#include "productresolver.h"

#include <queue>

namespace qbs::Internal {

void resolveProducts(LoaderState &loaderState)
{
    TopLevelProjectContext &topLevelProject = loaderState.topLevelProject();
    std::queue<std::pair<ProductContext *, int>> productsToHandle;
    for (ProjectContext * const projectContext : topLevelProject.projects()) {
        for (ProductContext &productContext : projectContext->products) {
            topLevelProject.addProductToHandle(productContext);
            productsToHandle.emplace(&productContext, -1);
        }
    }
    while (!productsToHandle.empty()) {
        const auto [product, queueSizeOnInsert] = productsToHandle.front();
        productsToHandle.pop();

        // If the queue of in-progress products has shrunk since the last time we tried handling
        // this product, there has been forward progress and we can allow a deferral.
        const Deferral deferral = queueSizeOnInsert == -1
                || queueSizeOnInsert > int(productsToHandle.size())
                ? Deferral::Allowed : Deferral::NotAllowed;
        loaderState.itemReader().setExtraSearchPathsStack(product->project->searchPathsStack);
        resolveProduct(*product, deferral, loaderState);
        if (topLevelProject.isCanceled())
            throw CancelException();

        // The search paths stack can change during dependency resolution (due to module providers);
        // check that we've rolled back all the changes
        QBS_CHECK(loaderState.itemReader().extraSearchPathsStack()
                  == product->project->searchPathsStack);

        // If we encountered a dependency to an in-progress product or to a bulk dependency,
        // we defer handling this product if it hasn't failed yet and there is still
        // forward progress.
        if (product->dependenciesResolvingPending())
            productsToHandle.emplace(product, int(productsToHandle.size()));
        else
            topLevelProject.removeProductToHandle(*product);

        topLevelProject.timingData() += product->timingData;
    }
}

} // namespace qbs::Internal
