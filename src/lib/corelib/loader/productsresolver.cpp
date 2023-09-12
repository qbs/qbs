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

#include <language/language.h>
#include <language/scriptengine.h>
#include <logging/categories.h>
#include <logging/translator.h>
#include <tools/profiling.h>
#include <tools/progressobserver.h>
#include <tools/setupprojectparameters.h>
#include <tools/stringconstants.h>

#include <algorithm>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <vector>

namespace qbs::Internal {
namespace {
struct ThreadInfo {
    ThreadInfo(std::future<void> &&future, LoaderState &loaderState)
        : future(std::move(future)), loaderState(loaderState)
    {}
    std::future<void> future;
    LoaderState &loaderState;
    bool done = false;
};

struct ProductWithLoaderState {
    ProductWithLoaderState(ProductContext &product, LoaderState *loaderState)
        : product(&product), loaderState(loaderState) {}
    ProductContext * const product;
    LoaderState *loaderState;
};

class ThreadsLocker {
public:
    ThreadsLocker(std::launch mode, std::mutex &mutex) {
        if (mode == std::launch::async)
            lock = std::make_unique<std::unique_lock<std::mutex>>(mutex);
    }
    std::unique_ptr<std::unique_lock<std::mutex>> lock;
};
} // namespace

class ProductsResolver
{
public:
    ProductsResolver(LoaderState &loaderState) : m_loaderState(loaderState) {}
    void resolve();

private:
    void initialize();
    void initializeProductQueue();
    void initializeLoaderStatePool();
    void runScheduler();
    void scheduleNext();
    bool tryToReserveLoaderState(ProductWithLoaderState &product, Deferral deferral);
    std::optional<std::pair<ProductContext *, Deferral>>
    unblockProductWaitingForLoaderState(LoaderState &loaderState);
    void startJob(const ProductWithLoaderState &product, Deferral deferral);
    void checkForCancelation();
    void handleFinishedThreads();
    void queueProductForScheduling(const ProductWithLoaderState &product, Deferral deferral);
    void waitForSingleDependency(const ProductWithLoaderState &product, ProductContext &dependency);
    void waitForBulkDependency(const ProductWithLoaderState &product);
    void unblockProductsWaitingForDependency(ProductContext &finishedProduct);
    void postProcess();
    void checkForMissedBulkDependencies(const ProductContext &product);

    static int dependsItemCount(const Item *item);
    static int dependsItemCount(ProductContext &product);

    LoaderState &m_loaderState;
    std::queue<std::pair<ProductWithLoaderState, int>> m_productsToSchedule;
    std::vector<ProductContext *> m_finishedProducts;
    std::unordered_map<ProductContext *,
    std::vector<ProductWithLoaderState>> m_waitingForSingleDependency;
    std::vector<ProductWithLoaderState> m_waitingForBulkDependency;
    std::unordered_map<LoaderState *, std::queue<std::pair<ProductContext *, Deferral>>> m_waitingForLoaderState;
    std::unordered_map<ProductContext *, ThreadInfo> m_runningThreads;
    std::mutex m_threadsMutex;
    std::condition_variable m_threadsNotifier;
    std::vector<std::unique_ptr<ScriptEngine>> m_enginePool;
    std::vector<std::unique_ptr<LoaderState>> m_loaderStatePool;
    std::vector<LoaderState *> m_availableLoaderStates;
    std::mutex m_cancelingMutex;
    std::launch m_asyncMode = std::launch::async;
    int m_maxJobCount = m_loaderState.parameters().maxJobCount();
    bool m_canceling = false;
};

void resolveProducts(LoaderState &loaderState)
{
    ProductsResolver(loaderState).resolve();
}

void ProductsResolver::resolve()
{
    initialize();
    try {
        runScheduler();
    } catch (const ErrorInfo &e) {
        for (auto &thread : m_runningThreads)
            thread.second.future.wait();
        throw e;
    }
    postProcess();
}

void ProductsResolver::initialize()
{
    initializeProductQueue();
    initializeLoaderStatePool();
}

void ProductsResolver::initializeProductQueue()
{
    TopLevelProjectContext &topLevelProject = m_loaderState.topLevelProject();
    std::vector<ProductContext *> sortedProducts;
    for (ProjectContext * const projectContext : topLevelProject.projects()) {
        for (ProductContext &product : projectContext->products) {
            topLevelProject.addProductToHandle(product);
            const auto it = std::lower_bound(sortedProducts.begin(), sortedProducts.end(), product,
                                             [&product](ProductContext *p1, const ProductContext &) {
                return dependsItemCount(*p1) < dependsItemCount(product);
            });
            sortedProducts.insert(it, &product);
        }
    }

    for (ProductContext * const product : sortedProducts) {
        queueProductForScheduling(ProductWithLoaderState(*product, nullptr), Deferral::Allowed);
        if (product->shadowProduct) {
            topLevelProject.addProductToHandle(*product->shadowProduct);
            queueProductForScheduling(ProductWithLoaderState(*product->shadowProduct, nullptr),
                                      Deferral::Allowed);
        }
    }
}

void ProductsResolver::initializeLoaderStatePool()
{
    TopLevelProjectContext &topLevelProject = m_loaderState.topLevelProject();

    // Adapt max job count: It makes no sense to have it be higher than the number of products
    // or what can actually be run concurrently. In both cases, we would simply waste resources.
    const int maxConcurrency = std::thread::hardware_concurrency();
    if (maxConcurrency > 0 && maxConcurrency < m_maxJobCount)
        m_maxJobCount = maxConcurrency;
    if (m_maxJobCount > topLevelProject.productsToHandleCount())
        m_maxJobCount = topLevelProject.productsToHandleCount();

    // The number of engines and loader states we need to allocate here is one less than the
    // total number of concurrent jobs, as we already have one loader state that we can re-use.
    if (m_maxJobCount > 1)
        m_enginePool.reserve(m_maxJobCount - 1);
    m_loaderStatePool.reserve(m_enginePool.size());
    m_availableLoaderStates.reserve(m_enginePool.size() + 1);
    m_availableLoaderStates.push_back(&m_loaderState);
    for (std::size_t i = 0; i < m_enginePool.capacity(); ++i) {
        ScriptEngine &engine = *m_enginePool.emplace_back(
            ScriptEngine::create(m_loaderState.logger(), EvalContext::PropertyEvaluation));
        ItemPool &itemPool = topLevelProject.createItemPool();
        engine.setEnvironment(m_loaderState.parameters().adjustedEnvironment());
        auto loaderState = std::make_unique<LoaderState>(
                    m_loaderState.parameters(), topLevelProject, itemPool, engine,
                    m_loaderState.logger());
        m_loaderStatePool.push_back(std::move(loaderState));
        m_availableLoaderStates.push_back(m_loaderStatePool.back().get());
        if (topLevelProject.progressObserver())
            topLevelProject.progressObserver()->addScriptEngine(m_enginePool.back().get());
    }
    qCDebug(lcLoaderScheduling) << "using" << m_availableLoaderStates.size() << "loader states";
    if (int(m_availableLoaderStates.size()) == 1)
        m_asyncMode = std::launch::deferred;
}

void ProductsResolver::runScheduler()
{
    AccumulatingTimer timer(m_loaderState.parameters().logElapsedTime()
                            ? &m_loaderState.topLevelProject().timingData().resolvingProducts
                            : nullptr);

    ThreadsLocker threadsLock(m_asyncMode, m_threadsMutex);
    while (true) {
        scheduleNext();
        if (m_runningThreads.empty())
            break;
        if (m_asyncMode == std::launch::async) {
            qCDebug(lcLoaderScheduling()) << "scheduling paused, waiting for threads to finish";
            m_threadsNotifier.wait(*threadsLock.lock);
        }
        checkForCancelation();
        handleFinishedThreads();
    }

    QBS_CHECK(m_productsToSchedule.empty());
    QBS_CHECK(m_loaderState.topLevelProject().productsToHandleCount() == 0);
    QBS_CHECK(m_runningThreads.empty());
    QBS_CHECK(m_waitingForSingleDependency.empty());
    QBS_CHECK(m_waitingForBulkDependency.empty());
}

void ProductsResolver::scheduleNext()
{
    TopLevelProjectContext &topLevelProject = m_loaderState.topLevelProject();
    AccumulatingTimer timer(m_loaderState.parameters().logElapsedTime()
                            ? &topLevelProject.timingData().schedulingProducts : nullptr);
    while (m_maxJobCount > int(m_runningThreads.size()) && !m_productsToSchedule.empty()) {
        auto [product, toHandleCountOnInsert] = m_productsToSchedule.front();
        m_productsToSchedule.pop();

        qCDebug(lcLoaderScheduling) << "potentially scheduling product"
                                    << product.product->displayName()
                                    << "unhandled product count on queue insertion"
                                    << toHandleCountOnInsert << "current unhandled product count"
                                    << topLevelProject.productsToHandleCount();

        // If the number of unfinished products has shrunk since the last time we tried handling
        // this product, there has been forward progress and we can allow a deferral.
        const Deferral deferral = toHandleCountOnInsert == -1
                || toHandleCountOnInsert > topLevelProject.productsToHandleCount()
                ? Deferral::Allowed : Deferral::NotAllowed;

        if (!tryToReserveLoaderState(product, deferral))
            continue;

        startJob(product, deferral);
    }

    // There are jobs running, so forward progress is still possible.
    if (!m_runningThreads.empty())
        return;

    QBS_CHECK(m_productsToSchedule.empty());

    // If we end up here, nothing was scheduled in the loop above, which means that either ...
    //  a) ... we are done or
    //  b) ... we finally need to schedule our bulk dependencies or
    //  c) ... we need to schedule products waiting for an unhandled dependency.
    // In the latter case, the project has at least one dependency cycle, and the
    // DependencyResolver will emit an error.

    // a)
    if (m_waitingForBulkDependency.empty() && m_waitingForSingleDependency.empty())
        return;

    // b)
    for (const ProductWithLoaderState &product : m_waitingForBulkDependency)
        queueProductForScheduling(product, Deferral::NotAllowed);
    if (!m_productsToSchedule.empty()) {
        m_waitingForBulkDependency.clear();
        scheduleNext();
        return;
    }

    // c)
    for (const auto &e : m_waitingForSingleDependency) {
        for (const ProductWithLoaderState &p : e.second)
            queueProductForScheduling(p, Deferral::NotAllowed);
    }
    QBS_CHECK(!m_productsToSchedule.empty());
    scheduleNext();
}

bool ProductsResolver::tryToReserveLoaderState(ProductWithLoaderState &product, Deferral deferral)
{
    QBS_CHECK(!m_availableLoaderStates.empty());
    if (!product.loaderState) {
        product.loaderState = m_availableLoaderStates.back();
        m_availableLoaderStates.pop_back();
        return true;
    }
    if (const auto it = std::find(m_availableLoaderStates.begin(), m_availableLoaderStates.end(),
                                  product.loaderState); it != m_availableLoaderStates.end()) {
        m_availableLoaderStates.erase(it);
        return true;
    }
    qCDebug(lcLoaderScheduling) << "loader state" << product.loaderState << " for product"
                                << product.product->displayName()
                                << "not available, adding product to wait queue";
    m_waitingForLoaderState[product.loaderState].push({product.product, deferral});
    return false;
}

std::optional<std::pair<ProductContext *, Deferral>>
ProductsResolver::unblockProductWaitingForLoaderState(LoaderState &loaderState)
{
    auto &waitingForLoaderState = m_waitingForLoaderState[&loaderState];
    if (waitingForLoaderState.empty())
        return {};
    const auto product = waitingForLoaderState.front();
    waitingForLoaderState.pop();
    qCDebug(lcLoaderScheduling) << "loader state" << &loaderState << "now available for product"
              << product.first->displayName();
    return product;
}

void ProductsResolver::startJob(const ProductWithLoaderState &product, Deferral deferral)
{
    QBS_CHECK(product.loaderState);
    qCDebug(lcLoaderScheduling) << "scheduling product" << product.product->displayName()
                                << "with loader state" << product.loaderState
                                << "and deferral mode" << int(deferral);
    try {
        const auto it = m_runningThreads.emplace(product.product, ThreadInfo(std::async(m_asyncMode,
            [this, product, deferral] {
                product.loaderState->itemReader().setExtraSearchPathsStack(
                product.product->project->searchPathsStack);
                resolveProduct(*product.product, deferral, *product.loaderState);

                // The search paths stack can change during dependency resolution
                // (due to module providers); check that we've rolled back all the changes
                QBS_CHECK(product.loaderState->itemReader().extraSearchPathsStack()
                          == product.product->project->searchPathsStack);

                std::lock_guard cancelingLock(m_cancelingMutex);
                if (m_canceling)
                    return;
                ThreadsLocker threadsLock(m_asyncMode, m_threadsMutex);
                if (const auto it = m_runningThreads.find(product.product);
                    it != m_runningThreads.end()) {
                    it->second.done = true;
                    qCDebug(lcLoaderScheduling) << "thread for product"
                                                << product.product->displayName()
                                                << "finished, waking up scheduler";
                    m_threadsNotifier.notify_one();
                }
            }), *product.loaderState));

        // With just one worker thread, the notify/wait overhead would be excessive, so
        // we run the task synchronously.
        if (m_asyncMode == std::launch::deferred) {
            qCDebug(lcLoaderScheduling) << "blocking on product thread immediately";
            it.first->second.future.wait();
        }
    } catch (const std::system_error &e) {
        if (e.code() != std::errc::resource_unavailable_try_again)
            throw e;
        qCWarning(lcLoaderScheduling) << "failed to create thread";
        if (m_maxJobCount >= 2) {
            m_maxJobCount /= 2;
            qCWarning(lcLoaderScheduling) << "throttling down to" << m_maxJobCount << "jobs";
        }
        queueProductForScheduling(product, deferral);
        m_availableLoaderStates.push_back(product.loaderState);
    }
}

void ProductsResolver::checkForCancelation()
{
    if (m_loaderState.topLevelProject().isCanceled()) {
        m_cancelingMutex.lock();
        m_canceling = true;
        m_cancelingMutex.unlock();
        for (auto &thread : m_runningThreads)
            thread.second.future.wait();
        throw CancelException();
    }
}

void ProductsResolver::handleFinishedThreads()
{
    TopLevelProjectContext &topLevelProject = m_loaderState.topLevelProject();
    AccumulatingTimer timer(m_loaderState.parameters().logElapsedTime()
                            ? &topLevelProject.timingData().schedulingProducts : nullptr);

    std::vector<std::pair<ProductWithLoaderState, Deferral>> productsToScheduleDirectly;
    for (auto it = m_runningThreads.begin(); it != m_runningThreads.end();) {
        ThreadInfo &ti = it->second;
        if (!ti.done) {
            ++it;
            continue;
        }
        ti.future.wait();
        ProductContext &product = *it->first;
        LoaderState &loaderState = ti.loaderState;
        it = m_runningThreads.erase(it);

        qCDebug(lcLoaderScheduling) << "handling finished thread for product"
                                    << product.displayName()
                                    << "current unhandled product count is"
                                    << topLevelProject.productsToHandleCount();

        // If there are products waiting for the loader state used in the finished thread,
        // we can start a job for one of them right away (but not in the loop,
        // because startJob() modifies the thread list we are currently iterating over).
        if (const auto productInfo = unblockProductWaitingForLoaderState(loaderState)) {
            productsToScheduleDirectly.emplace_back(
                        ProductWithLoaderState(*productInfo->first, &loaderState),
                        productInfo->second);
        } else {
            qCDebug(lcLoaderScheduling) << "making loader state" << &loaderState
                                        << "available again";
            m_availableLoaderStates.push_back(&loaderState);
        }

        // If we encountered a dependency to an in-progress product or to a bulk dependency,
        // we defer handling this product.
        if (product.dependenciesResolvingPending()) {
            qCDebug(lcLoaderScheduling) << "dependencies resolving not finished for product"
                                        << product.displayName();
            const auto pending = product.pendingDependency();
            switch (pending.first) {
            case ProductDependency::Single:
                waitForSingleDependency(ProductWithLoaderState(product, &loaderState),
                                        *pending.second);
                break;
            case ProductDependency::Bulk:
                waitForBulkDependency(ProductWithLoaderState(product, &loaderState));
                break;
            case ProductDependency::None:
                // This can happen if the dependency has finished in between the check in
                // DependencyResolver and the one here.
                QBS_CHECK(pending.second);
                queueProductForScheduling(ProductWithLoaderState(product, &loaderState),
                                          Deferral::Allowed);
                break;
            }
            topLevelProject.incProductDeferrals();
        } else {
            qCDebug(lcLoaderScheduling) << "product" << product.displayName() << "finished";
            topLevelProject.removeProductToHandle(product);
            if (!product.name.startsWith(StringConstants::shadowProductPrefix()))
                m_finishedProducts.push_back(&product);
            topLevelProject.timingData() += product.timingData;
            checkForMissedBulkDependencies(product);
            topLevelProject.registerBulkDependencies(product);
            unblockProductsWaitingForDependency(product);
        }
    }

    for (const auto &productInfo : productsToScheduleDirectly)
        startJob(productInfo.first, productInfo.second);
}

void ProductsResolver::queueProductForScheduling(const ProductWithLoaderState &product,
                                                 Deferral deferral)
{
    qCDebug(lcLoaderScheduling) << "queueing product" << product.product->displayName()
                                << "with deferral mode" << int(deferral);
    m_productsToSchedule.emplace(product, deferral == Deferral::Allowed
            ? -1 : m_loaderState.topLevelProject().productsToHandleCount());
}

void ProductsResolver::waitForSingleDependency(const ProductWithLoaderState &product,
                                               ProductContext &dependency)
{
    qCDebug(lcLoaderScheduling) << "product" << product.product->displayName()
                                << "now waiting for single dependency"
                                << dependency.displayName();
    m_waitingForSingleDependency[&dependency].push_back(product);
}

void ProductsResolver::waitForBulkDependency(const ProductWithLoaderState &product)
{
    qCDebug(lcLoaderScheduling) << "product" << product.product->displayName()
                                << "now waiting for bulk dependency";
    m_waitingForBulkDependency.push_back(product);
}

void ProductsResolver::unblockProductsWaitingForDependency(ProductContext &finishedProduct)
{
    const auto it = m_waitingForSingleDependency.find(&finishedProduct);
    if (it == m_waitingForSingleDependency.end())
        return;

    qCDebug(lcLoaderScheduling) << "unblocking all products waiting for now-finished product" <<
                                   finishedProduct.displayName();
    for (const ProductWithLoaderState &p : it->second) {
        qCDebug(lcLoaderScheduling) << "  unblocking product" << p.product->displayName();
        queueProductForScheduling(p, Deferral::Allowed);
    }
    m_waitingForSingleDependency.erase(it);
}

void ProductsResolver::postProcess()
{
    for (ProductContext * const product : m_finishedProducts) {
        if (product->product)
            product->product->project->products.push_back(product->product);

        // This has to be done in post-processing, because we need both product and shadow product
        // to be ready, and contrary to what one might assume, there is no proper ordering
        // between them regarding dependency resolving.
        setupExports(*product, m_loaderState);
    }

    for (const auto &engine : m_enginePool)
        m_loaderState.topLevelProject().collectDataFromEngine(*engine);

    QBS_CHECK(!m_loaderState.topLevelProject().projects().empty());
    const auto project = std::dynamic_pointer_cast<TopLevelProject>(
                m_loaderState.topLevelProject().projects().front()->project);
    QBS_CHECK(project);
    for (LoaderState * const loaderState : m_availableLoaderStates)
        project->warningsEncountered << loaderState->logger().warnings();
}

void ProductsResolver::checkForMissedBulkDependencies(const ProductContext &product)
{
    if (!product.product || !product.product->enabled || !product.bulkDependencies.empty())
        return;
    for (const FileTag &tag : product.product->fileTags) {
        for (const auto &[p, location]
             : m_loaderState.topLevelProject().finishedProductsWithBulkDependency(tag)) {
            if (!p->product->enabled)
                continue;
            if (p->name == product.name)
                continue;
            ErrorInfo e;
            e.append(Tr::tr("Cyclic dependencies detected:"));
            e.append(Tr::tr("First product is '%1'.")
                     .arg(product.displayName()), product.item->location());
            e.append(Tr::tr("Second product is '%1'.")
                     .arg(p->displayName()), p->item->location());
            e.append(Tr::tr("Dependency from %1 to %2 was established via Depends.productTypes.")
                     .arg(p->displayName(), product.displayName()), location);
            if (m_loaderState.parameters().productErrorMode() == ErrorHandlingMode::Strict)
                throw e;
            m_loaderState.logger().printWarning(e);
            m_loaderState.logger().printWarning(
                        ErrorInfo(Tr::tr("Product '%1' had errors and was disabled.")
                                  .arg(product.displayName()), product.item->location()));
            m_loaderState.logger().printWarning(
                        ErrorInfo(Tr::tr("Product '%1' had errors and was disabled.")
                                  .arg(p->displayName()), p->item->location()));
            product.product->enabled = false;
            p->product->enabled = false;
        }
    }
}

int ProductsResolver::dependsItemCount(const Item *item)
{
    int count = 0;
    for (const Item * const child : item->children()) {
        if (child->type() == ItemType::Depends)
            ++count;
    }
    return count;
}

int ProductsResolver::dependsItemCount(ProductContext &product)
{
    if (product.dependsItemCount == -1)
        product.dependsItemCount = dependsItemCount(product.item);
    return product.dependsItemCount;
}

} // namespace qbs::Internal
