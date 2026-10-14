// In-app purchase (apps/common/iap.h): the iPhone and iPad backend, StoreKit 2 (iOS 15+), for both apps.
//
// One non-consumable product, "unlock everything". At iap_init: the product is asked for (its price), the device's
// transactions are checked again (Transaction.currentEntitlements: a purchase made on another device, restored, or
// refunded since), and Transaction.updates is listened to for the rest of the run (Ask to Buy approved later, a
// refund, a purchase made outside the app). StoreKit 2 verifies each transaction's signature itself; an unverified
// one counts for nothing.
//
// The C API (iap.h) is called from the app's main loop, on the main thread; the StoreKit work runs in Swift tasks and
// hands its results back to the main thread (DispatchQueue.main, which SDL's event pump runs every frame), so the state
// below is only ever read and written there. Swift 5 language mode (SWIFT_VERSION in apps/ios/<app>/project.yml).
// MIT licence, (c) 2026 Pierre-Louis Boyer (8BCraft).

import Foundation
import StoreKit

private final class Store {
    static let shared = Store()

    var productID = ""
    var product: Product?
    var unlocked = false
    var busy = false
    var loading = false
    var changes: UInt32 = 0
    var price: UnsafeMutablePointer<CChar> = strdup("")!   // iap_price_text(): valid until the next change
    var priceAscii: UnsafeMutablePointer<CChar> = strdup("")!   // iap_price_ascii()
    var updates: Task<Void, Never>?

    func bump() { changes &+= 1 }

    func setPrice(_ p: Product) {
        let old = price, oldAscii = priceAscii
        price = strdup(Store.plain(p.displayPrice)) ?? strdup("")!
        priceAscii = strdup(Store.ascii(p)) ?? strdup("")!
        free(old)
        free(oldAscii)
    }

    // the price with its currency's ISO code instead of its sign, in the store's number format: "4,99 EUR"
    static func ascii(_ p: Product) -> String {
        let f = NumberFormatter()
        f.numberStyle = .currencyISOCode
        f.locale = p.priceFormatStyle.locale
        f.currencyCode = p.priceFormatStyle.currencyCode
        let s = plain(f.string(from: NSDecimalNumber(decimal: p.price)) ?? "")
        return String(String.UnicodeScalarView(s.unicodeScalars.filter { $0.value >= 0x20 && $0.value < 0x7F }))
            .trimmingCharacters(in: .whitespaces)
    }

    func start(_ id: String, unlockedBefore: Bool) {
        guard updates == nil else { return }
        productID = id
        unlocked = unlockedBefore
        guard !id.isEmpty else { return }
        updates = Task.detached {
            for await result in Transaction.updates {
                await Store.shared.apply(result)
            }
        }
        load()
        Task.detached { await Store.shared.checkEntitlements() }
    }

    // the product (its price); asked again when the panel's button finds it missing
    func load() {
        guard !loading, product == nil, !productID.isEmpty else { return }
        loading = true
        let id = productID
        Task.detached {
            var found: Product?
            do {
                found = try await Product.products(for: [id]).first
                if found == nil { NSLog("iap: no product %@ (App Store Connect: its id, its agreements)", id) }
            } catch {
                NSLog("iap: the products could not be loaded: %@", String(describing: error))
            }
            let loaded = found              // (a constant: the main queue's closure is @Sendable)
            DispatchQueue.main.async {
                let s = Store.shared
                s.loading = false
                if let p = loaded {
                    s.product = p
                    s.setPrice(p)
                    s.bump()
                }
            }
        }
    }

    // the device's transactions: is the product owned now? (bought, restored, or refunded since)
    func checkEntitlements() async {
        let id = productID
        var owned = false
        for await result in Transaction.currentEntitlements {
            if case .verified(let t) = result, t.productID == id, t.revocationDate == nil { owned = true }
        }
        let now = owned
        DispatchQueue.main.async {
            let s = Store.shared
            if s.unlocked != now {
                s.unlocked = now
                s.bump()
            }
        }
    }

    // a transaction from Transaction.updates (or a purchase): it unlocks, or a refund locks again
    func apply(_ result: VerificationResult<Transaction>) async {
        guard case .verified(let t) = result else { return }
        if t.productID == productID {
            let owned = t.revocationDate == nil
            DispatchQueue.main.async {
                let s = Store.shared
                if s.unlocked != owned {
                    s.unlocked = owned
                    s.bump()
                }
            }
        }
        await t.finish()
    }

    func buy() -> Bool {
        guard !busy else { return false }
        guard let p = product else {
            load()
            return false
        }
        busy = true
        bump()
        Task.detached {
            do {
                let r = try await p.purchase()
                switch r {
                case .success(let v): await Store.shared.apply(v)
                case .pending: NSLog("iap: the purchase waits for approval (Ask to Buy)")
                case .userCancelled: break
                @unknown default: break
                }
            } catch {
                NSLog("iap: the purchase failed: %@", String(describing: error))
            }
            DispatchQueue.main.async {
                Store.shared.busy = false
                Store.shared.bump()
            }
        }
        return true
    }

    func restore() {
        guard !busy, !productID.isEmpty else { return }
        busy = true
        bump()
        Task.detached {
            do {
                try await AppStore.sync()   // the account's purchases (it may ask to sign in)
            } catch {
                NSLog("iap: restore: %@", String(describing: error))
            }
            await Store.shared.checkEntitlements()
            DispatchQueue.main.async {
                Store.shared.busy = false
                Store.shared.bump()
            }
        }
        load()
    }

    // the price as the app's own font can draw it: Latin digits, plain spaces, no direction marks
    static func plain(_ s: String) -> String {
        var out = ""
        for u in s.unicodeScalars {
            switch u.value {
            case 0x0660...0x0669: out.unicodeScalars.append(UnicodeScalar(u.value - 0x0660 + 0x30)!)
            case 0x06F0...0x06F9: out.unicodeScalars.append(UnicodeScalar(u.value - 0x06F0 + 0x30)!)
            case 0x0966...0x096F: out.unicodeScalars.append(UnicodeScalar(u.value - 0x0966 + 0x30)!)
            case 0x066B: out.append(".")
            case 0x066C: out.append(",")
            case 0x00A0, 0x2007, 0x2009, 0x202F: out.append(" ")
            case 0x200E, 0x200F, 0x061C, 0x202A...0x202E, 0x2066...0x2069: break
            default: out.unicodeScalars.append(u)
            }
        }
        return out.trimmingCharacters(in: .whitespaces)
    }
}

@_cdecl("iap_init")
public func iap_init(_ productID: UnsafePointer<CChar>?, _ unlockedBefore: Int32) {
    let id = productID.map { String(cString: $0) } ?? ""
    Store.shared.start(id, unlockedBefore: unlockedBefore != 0)
}

@_cdecl("iap_available")
public func iap_available() -> Int32 {
    return Store.shared.productID.isEmpty ? 0 : 1
}

@_cdecl("iap_unlocked")
public func iap_unlocked() -> Int32 {
    return Store.shared.unlocked ? 1 : 0
}

@_cdecl("iap_price_text")
public func iap_price_text() -> UnsafePointer<CChar> {
    return UnsafePointer(Store.shared.price)
}

@_cdecl("iap_price_ascii")
public func iap_price_ascii() -> UnsafePointer<CChar> {
    return UnsafePointer(Store.shared.priceAscii)
}

@_cdecl("iap_busy")
public func iap_busy() -> Int32 {
    return Store.shared.busy ? 1 : 0
}

@_cdecl("iap_buy")
public func iap_buy() -> Int32 {
    return Store.shared.buy() ? 1 : 0
}

@_cdecl("iap_restore")
public func iap_restore() {
    Store.shared.restore()
}

@_cdecl("iap_changes")
public func iap_changes() -> UInt32 {
    return Store.shared.changes
}
