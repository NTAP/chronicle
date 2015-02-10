--- if_e1000e_netmap.h	2013-04-18 16:33:18.961836000 -0400
+++ CHRONICLE/e1000e/if_e1000e_netmap.h	2013-04-18 16:34:17.943665000 -0400
@@ -213,13 +213,21 @@ e1000_netmap_rxsync(struct ifnet *ifp, u
 	j = netmap_idx_n2k(kring, l);
 	if (netmap_no_pendintr || force_update) {
 		for (n = 0; ; n++) {
+			/* CHRONICLE:
 			struct e1000_rx_desc *curr = E1000_RX_DESC(*rxr, l);
 			uint32_t staterr = le32toh(curr->status);
+			*/
+			union e1000_rx_desc_extended *curr = E1000_RX_DESC_EXT(*rxr, l);
+			uint32_t staterr = le32toh(curr->wb.upper.status_error);
 
 			if ((staterr & E1000_RXD_STAT_DD) == 0)
 				break;
+			/* CHRONICLE: 
 			ring->slot[j].len = le16toh(curr->length) - strip_crc;
+			*/
+			ring->slot[j].len = le16toh(curr->wb.upper.length) - strip_crc;
 			ring->slot[j].flags = NS_FORWARD;
+			
 			j = (j == lim) ? 0 : j + 1;
 			l = (l == lim) ? 0 : l + 1;
 		}
@@ -243,7 +251,8 @@ e1000_netmap_rxsync(struct ifnet *ifp, u
 		l = netmap_idx_k2n(kring, j); /* NIC ring index */
 		for (n = 0; j != k; n++) {
 			struct netmap_slot *slot = &ring->slot[j];
-			struct e1000_rx_desc *curr = E1000_RX_DESC(*rxr, j);
+			//CHRONICLE: struct e1000_rx_desc *curr = E1000_RX_DESC(*rxr, j);
+			union e1000_rx_desc_extended *curr = E1000_RX_DESC_EXT(*rxr, j);
 			uint64_t paddr;
 			void *addr = PNMB(slot, &paddr);
 
@@ -252,12 +261,15 @@ e1000_netmap_rxsync(struct ifnet *ifp, u
 					mtx_unlock(&kring->q_lock);
 				return netmap_ring_reinit(kring);
 			}
-			if (slot->flags & NS_BUF_CHANGED) {
+			//CHRONICLE: if (slot->flags & NS_BUF_CHANGED) {
+			if (1) {
 				// netmap_reload_map(pdev, DMA_TO_DEVICE, old_paddr, addr)
-				curr->buffer_addr = htole64(paddr);
+				//CHRONICLE: curr->buffer_addr = htole64(paddr);
+				curr->read.buffer_addr = htole64(paddr);
 				slot->flags &= ~NS_BUF_CHANGED;
 			}
-			curr->status = 0;
+			//CHRONICLE: curr->status = 0;
+			curr->wb.upper.status_error = 0;
 			j = (j == lim) ? 0 : j + 1;
 			l = (l == lim) ? 0 : l + 1;
 		}
@@ -315,7 +327,8 @@ static int e1000e_netmap_init_buffers(st
 			D("rx buf %d was set", i);
 		bi->skb = NULL; // XXX leak if set
 		// netmap_load_map(...)
-		E1000_RX_DESC(*rxr, i)->buffer_addr = htole64(paddr);
+		//CHRONICLE: E1000_RX_DESC(*rxr, i)->buffer_addr = htole64(paddr);
+		E1000_RX_DESC_EXT(*rxr, i)->read.buffer_addr = htole64(paddr);
 	}
 	rxr->next_to_use = 0;
 	/* preserve buffers already made available to clients */
